#ifdef _CH_
#pragma package <opencv>
#endif

#include "cv.h"
#include "highgui.h"
#include <stdio.h>
#include <stdlib.h>
#include "../utilities.h"
#include <string.h>
#include <iostream>
#include <vector>

#define NUM_IMAGES 2
#define THRESHOLD 55

using namespace std;
using namespace cv;

//Region of interest class
//This is used when cropping out the images from the
//connected components image. I needed a class so that
//it would keep track of the x and y of the original
//image so I knew where to place the templates when
//a successful match has occurred with the sample
class ROI {
	public:
		int x;
		int y;
		IplImage * image;
	public:
		//Getters
		int getX(){
			return x;
		}
		int getY(){
			return y;
		}
};

//This class was used when a match had occurred between
//a template and a sample. I wanted to keep track of
//the difference and also the percentage of points
//that matched so I could better facilitate a successful match
class Match {
	public:
		IplImage * _template;
		unsigned int difference;
		int percentageMatch;
	public:
		//Getters
		int getDifference(){
			return difference;
		}
		int getPercentageMatch(){
			return percentageMatch;
		}
};

vector<unsigned char> encounteredColours;//This vector is used ensure that when scanning the connected components image that no one colour is counted twice
vector<IplImage *> templates;//vector store all the template images
vector<IplImage *> samples;//vector to store the samples

//Convert to grayscale method
void convertToGrayscale(IplImage * source, IplImage * result) {
	cvCvtColor(source, result, CV_RGB2GRAY);
}

// Locate the red pixels in the source image.
// TO DO:  Write code to select all the red road sign points.  You may need to clean up the result
// using mathematical morphology.  The result should be a binary image with the selected red
// points as white points. The temp image passed may be used in your processing.
void find_red_points(IplImage * source, IplImage * result, IplImage * temp) {
//	int width_step = source->widthStep;
//	int pixel_step = source->widthStep/source->width;
//	int number_channels = source->nChannels;
//	cvZero(result);
//
//	unsigned char white_pixel[4] = {255,255,255,0};
//	int row, col;
//
//	// Find all red points in the image
//	for (row=0; row < result->height; row++) {
//		for (col=0; col < result->width; col++) {
//			unsigned char* curr_point = GETPIXELPTRMACRO( source, col, row, width_step, pixel_step );
//
//			if ((curr_point[RED_CH] >= THRESHOLD) && (curr_point[RED_CH] > curr_point[BLUE_CH]) && (curr_point[RED_CH] > curr_point[GREEN_CH])) {
//				PUTPIXELMACRO(result, col, row, white_pixel, width_step, pixel_step, number_channels);
//			}
//
//		}
//	}
//
//	int filterSize = 6;
//	IplConvKernel * structureElement = cvCreateStructuringElementEx(filterSize, filterSize, (filterSize - 1) / 2, (filterSize - 1) / 2, CV_SHAPE_RECT, NULL);
//
//	//Apply morphological opening and closing operations to clean up the image
//	//Perform opening
//	cvMorphologyEx(result, temp, NULL, structureElement, CV_MOP_OPEN, 1);
//	//Perform closing
//	cvMorphologyEx(temp, result, NULL, structureElement, CV_MOP_CLOSE, 1);

	IplImage *hsv, *hue, *sat, *val;
	int depth;
	CvSize size;

	size = cvGetSize(source);
	depth = source->depth;
	hue = cvCreateImage(size, depth, 1);
	sat = cvCreateImage(size, depth, 1);
	val = cvCreateImage(size, depth, 1);
	hsv = cvCreateImage(size, depth, 3);
	cvZero(hue);
	cvZero(sat);
	cvZero(val);
	cvZero(hsv);

	//Convert from BGR to HSV
	cvCvtColor(source, hsv, CV_BGR2HSV);

	//Split HSV into their individual channels
	cvSplit(hsv, hue, sat, val, 0);

	int hue_width_step = hue->widthStep;
	int hue_pixel_step = hue->widthStep/hue->width;

	int sat_width_step = sat->widthStep;
	int sat_pixel_step = sat->widthStep/sat->width;

	int val_width_step = val->widthStep;
	int val_pixel_step = val->widthStep/val->width;

	cvZero(result);
	int result_width_step = result->widthStep;
	int result_pixel_step = result->widthStep/result->width;
	int result_channels = result->nChannels;

	unsigned char white_pixel[4] = {255,255,255,0};

	int row, col;
	// Find all red points in the image by going through the HSV channels
	for (row=0; row < result->height; row++) {
		for (col=0; col < result->width; col++) {
			unsigned char * hue_point = GETPIXELPTRMACRO(hue, col, row, hue_width_step, hue_pixel_step);
			unsigned char * sat_point = GETPIXELPTRMACRO(sat, col, row, sat_width_step, sat_pixel_step);
			unsigned char * val_point = GETPIXELPTRMACRO(val, col, row, val_width_step, val_pixel_step);

			//This condition catches the red points using HSV (the above commented out section uses BGR but wasn't as good)
			//This was obtained through observing the written pixel values on the image and coming up with a good combination of thresholds
			if((hue_point[0] >= 160  || hue_point[0] <= 10) && sat_point[0] >= 60 && val_point[0] >= 60) {
				PUTPIXELMACRO(result, col, row, white_pixel, result_width_step, result_pixel_step, result_channels);
			}
		}
	}

	//Structuring element
	//int filterSize = 6;
	//IplConvKernel * structureElement = cvCreateStructuringElementEx(filterSize, filterSize, (filterSize - 1) / 2, (filterSize - 1) / 2, CV_SHAPE_RECT, NULL);

	//Perform close
	cvMorphologyEx(result, result, NULL, NULL, CV_MOP_CLOSE, 1);
}

//This is the method I use to place the template on the original image
void overlayImage(IplImage * dest, IplImage * clip, int x, int y) {
	//Setup image size
	IplImage * _template = cvCreateImage((cvSize(clip->width/2, clip->height/2)), clip->depth, clip->nChannels);
	//Resize clip to same size as destination
	cvResize(clip, _template, 1);

	//Set the region of interest to the size of the destination image
	cvSetImageROI(dest, cvRect(x, y, _template->width, _template->height));

	//Place the template image onto the destination
	cvCopyImage(_template, dest);
	cvResetImageROI(dest);
}

//This is the method I use to crop out the templates and samples
IplImage* crop(IplImage* input, int top, int left, int width, int height) {
	IplImage* result;

	CvSize size;

	//Defensive
	if (input->width <= 0 || input->height <= 0 || width <= 0 || height <= 0) {
		exit(1);
	}

	//Defensive
	if (input->depth != IPL_DEPTH_8U) {
		exit(1);
	}

	//Create rectangular region to use to crop the ROI
	CvRect region;
	region.x = left;
	region.y = top;
	region.width = width;
	region.height = height;

	//Set the desired region of interest.
	cvSetImageROI(input, region);
	//Copy region of interest into a new image
	size.width = width;
	size.height = height;
	result = cvCreateImage(size, IPL_DEPTH_8U, input->nChannels);

	cvCopy(input, result);// Copy the region
	return result;
}

//This method is used to resize the samples to the same size as the template
//during the template matching stage
IplImage* resizeImage(const IplImage *origImg, int newWidth, int newHeight) {
	IplImage *outImg = 0;
	int origWidth;
	int origHeight;

	//Defensive
	if (origImg) {
		origWidth = origImg->width;
		origHeight = origImg->height;
	}

	if (newWidth <= 0 || newHeight <= 0 || origImg == 0
		|| origWidth <= 0 || origHeight <= 0) {
		exit(1);
	}

	//Scale the image to the new dimensions even if the aspect ratio will be changed.
	outImg = cvCreateImage(cvSize(newWidth, newHeight), origImg->depth, origImg->nChannels);

	if (newWidth > origImg->width && newHeight > origImg->height) {
		// Make the image larger
		cvResetImageROI((IplImage*)origImg);
		cvResize(origImg, outImg, CV_INTER_LINEAR);
	} else {
		// Make the image smaller
		cvResetImageROI((IplImage*)origImg);
		cvResize(origImg, outImg, CV_INTER_AREA);
	}
	return outImg;
}

//This method is used to tell if supplied pixel value has already been encountered
//Makes use of the encountered colours vector
int encounteredColour(unsigned char * pixel) {
	vector<unsigned char>::iterator it;
	it = encounteredColours.begin();

	//Vector is empty
	if(it == encounteredColours.end()) {
		return 0;
	}

	//Pixel has been found in vector
	if(find(encounteredColours.begin(), encounteredColours.end(), pixel[0])!=encounteredColours.end()) {
		return 1;
	}

	return 0;//Have not encountered this colour before
}

//This method is used to scan through the connected components image
//and crop out each of the signs that it finds and adds it to a vector
//so that we can use it in the future.
//This method is used on both the templates and the samples
vector<ROI> extractSign(IplImage * connectedComponents, IplImage * red_point_image) {
	vector<ROI> images;

	int width_step = connectedComponents->widthStep;
	int pixel_step = connectedComponents->widthStep/connectedComponents->width;

	int row, col;
	int top=0, left=connectedComponents->width, right=0, bottom=0;

	encounteredColours.clear();//Empty the vector

	// Scan image
	for (row=0; row < connectedComponents->height; row++) {
		for (col=0; col < connectedComponents->width; col++) {
			unsigned char* curr_point = GETPIXELPTRMACRO(connectedComponents, col, row, width_step, pixel_step);

			if(curr_point[0] != 0) {//If current point is not a black pixel
				unsigned char* coloured_pixel = curr_point;//save the colour

				if(encounteredColour(coloured_pixel) == 0) {//If the colour has not been encountered already
					top = row;//encountered first part of new colour so save the top coordinate

					//Process shape
					for (int i=row; i < connectedComponents->height; i++) {
						for (int j=0; j < connectedComponents->width; j++) {

							curr_point = GETPIXELPTRMACRO(connectedComponents, j, i, width_step, pixel_step);

							//If same as previous colour update shape variables
							//The idea here is to get the perfect shape to crop out the image
							if(curr_point[0] == coloured_pixel[0]) {
								if(j > right) {
									right = j;
								}

								if(j < left) {
									left = j;
								}

								bottom = i;
							}
						}//end for
					}//end for

					encounteredColours.push_back(coloured_pixel[0]);//add colour so it won't be counted again
					int width = right - left;
					int height = bottom - top;

					//Only process images that are large enough to be the templates
					if(width > 10 && height > 10) {//There are some outliers to be caught here (1 or 2 single pixel values)

						//Crop out the image given the shape coordinates
						IplImage * croppedImage = crop(red_point_image, top, left, width, height);

						ROI roi;
						roi.x = left;
						roi.y = top;
						roi.image = croppedImage;

						//Add image to vector so we can process for template matching later
						images.push_back(roi);
					}

					//Reset shape variables
					top = 0;
					bottom = 0;
					left = connectedComponents->width;
					right = 0;
					//Reset loop variables
					row = 0;
					col = 0;

				}//end if
			}//end if
		}//end for
	}//end for

	return images;
}

//This method was supplied -> its result image is used to crop out the templates and samples
CvSeq * connected_components(IplImage * source, IplImage * result) {
	//Convert the source image to binary image
	IplImage * binary_image = cvCreateImage(cvGetSize(source), 8, 1);
	cvConvertImage(source, binary_image);

	CvMemStorage * storage = cvCreateMemStorage(0);
	CvSeq * contours = 0;

	cvThreshold(binary_image, binary_image, 1, 255, CV_THRESH_BINARY);
	cvFindContours(binary_image, storage, &contours, sizeof(CvContour), CV_RETR_CCOMP, CV_CHAIN_APPROX_SIMPLE);

	int offset = 40;

	if (result) {
		cvZero(result);
		for (CvSeq* contour = contours; contour != 0; contour = contour->h_next) {
			CvScalar color = CV_RGB(offset, offset, offset);
			/* replace CV_FILLED with 1 to see the outlines */
			cvDrawContours(result, contour, color, color, -1, CV_FILLED, 8);
			offset += 15;
		}
	}
	return contours;
}

// TO DO:  Write code to invert all points in the source image (i.e. for each channel, for each pixel in the result
// image, the value should be 255 less the corresponding value in the source image).
void invert_image(IplImage * source, IplImage * result) {
	int width_step = source->widthStep;
	int pixel_step = source->widthStep / source->width;

	int result_width_step = result->widthStep;
	int result_pixel_step = result->widthStep / result->width;
	int result_channels = result->nChannels;
	cvZero(result);

	int row, col;

	// Invert image
	for (row = 0; row < result->height; row++) {
		for (col = 0; col < result->width; col++) {
			unsigned char * curr_point = GETPIXELPTRMACRO(source, col, row, width_step, pixel_step);//Get current point in image
			unsigned char * result_point = GETPIXELPTRMACRO(result, col, row, result_width_step, result_pixel_step);

			//Subtract current point from 255
			for(int channel=0;channel<result_channels;channel++) {
				result_point[channel] = 255 - curr_point[channel];
			}

			PUTPIXELMACRO(result, col, row, result_point, result_width_step, result_pixel_step, result_channels);
		}
	}
}

// Assumes a 1D histogram of 256 elements
// TO DO: Given a 1-D CvHistogram you need to determine and return the optimal threshold value.
int determine_optimal_threshold(CvHistogram * hist) {
	int currentThreshold = 127;
	int prevThreshold;

	int i;

	do {
		prevThreshold = currentThreshold;

		int numBackgroundPixels = 0, numObjectPixels = 0;
		int backgroundPixelsSum = 0, objectPixelsSum = 0;

		//Background
		for(i=0;i<currentThreshold;i++) {
			int histogramValue = ((int) * cvGetHistValue_1D(hist, i));
			backgroundPixelsSum += (i*histogramValue);
			numBackgroundPixels += histogramValue;
		}

		//Object
		for(;i<256;i++) {
			int histogramValue = ((int) * cvGetHistValue_1D(hist, i));
			objectPixelsSum += (i*histogramValue);
			numObjectPixels += histogramValue;
		}

		int uO = (numObjectPixels == 0) ? 0 : (objectPixelsSum / numObjectPixels);
		int uB = (numBackgroundPixels == 0) ? 0 : (backgroundPixelsSum / numBackgroundPixels);

		currentThreshold = (uO + uB) / 2;//update

	} while(currentThreshold != prevThreshold);

	return currentThreshold;
}

// TO DO:  Apply binary thresholding to those points in the grayscale_image which correspond to non-zero
// points in the passed mask_image.  The binary results (0 or 255) should be stored in the result_image.
void apply_threshold_with_mask(IplImage * grayscale_image, IplImage * result_image, IplImage * mask_image, int threshold) {
	int result_width_step = result_image->widthStep;
	int result_pixel_step = result_image->widthStep / result_image->width;
	int result_channels = result_image->nChannels;

	unsigned char white_pixel[4] = {255, 255, 255, 0};
	unsigned char black_pixel[4] = {0, 0, 0, 0};
	int row, col;

	// Loop through image
	for (row = 0; row < grayscale_image->height; row++) {
		for (col = 0; col < grayscale_image->width; col++) {

			//If mask is non-zero
			if(CV_IMAGE_ELEM(mask_image, uchar, row, col) != 0) {

				//Apply binary thresholding to gray scale image points where the corresponding
				//mask image points are non-zero
				if(CV_IMAGE_ELEM(grayscale_image, uchar, row, col)  >= threshold) {
					//White
					PUTPIXELMACRO(result_image, col, row, white_pixel, result_width_step, result_pixel_step, result_channels);
				} else {
					PUTPIXELMACRO(result_image, col, row, black_pixel, result_width_step, result_pixel_step, result_channels);
				}

			}
		}
	}
}

//This method was supplied
void determine_optimal_sign_classification(IplImage* original_image,
		IplImage* red_point_image, CvSeq* red_components,
		CvSeq* background_components, IplImage* result_image) {
	int width_step = original_image->widthStep;
	int pixel_step = original_image->widthStep / original_image->width;

	IplImage* mask_image = cvCreateImage(cvGetSize(original_image), 8, 1);
	IplImage* grayscale_image = cvCreateImage(cvGetSize(original_image), 8, 1);

	cvConvertImage(original_image, grayscale_image);
	IplImage* thresholded_image = cvCreateImage(cvGetSize(original_image), 8, 1);
	cvZero(thresholded_image);
	cvZero(result_image);

	int row = 0, col = 0;
	CvSeq* curr_red_region = red_components;
	// For every connected red component
	while (curr_red_region != NULL) {
		cvZero(mask_image);
		CvScalar color = CV_RGB( 255, 255, 255 );
		CvScalar mask_value = cvScalar(255);
		// Determine which background components are contained within the red component (i.e. holes)
		//  and create a binary mask of those background components.
		CvSeq* curr_background_region = curr_red_region->v_next;

		if (curr_background_region != NULL) {
			while (curr_background_region != NULL) {
				cvDrawContours(mask_image, curr_background_region, mask_value, mask_value, -1, CV_FILLED, 8);
				cvDrawContours(result_image, curr_background_region, color, color, -1, CV_FILLED, 8);
				curr_background_region = curr_background_region->h_next;
			}

			int hist_size = 256;
			CvHistogram* hist = cvCreateHist(1, &hist_size, CV_HIST_ARRAY);
			cvCalcHist(&grayscale_image, hist, 0, mask_image);
			// Determine an optimal threshold on the points within those components (using the mask)
			int optimal_threshold = determine_optimal_threshold(hist);
			apply_threshold_with_mask(grayscale_image, result_image, mask_image, optimal_threshold);
		}
		curr_red_region = curr_red_region->h_next;
	}

	for (row = 0; row < result_image->height; row++) {
		unsigned char* curr_red = GETPIXELPTRMACRO( red_point_image, 0, row, width_step, pixel_step );
		unsigned char* curr_result = GETPIXELPTRMACRO( result_image, 0, row, width_step, pixel_step );

		for (col = 0; col < result_image->width; col++) {
			curr_red += pixel_step;
			curr_result += pixel_step;

			if (curr_red[0] > 0) {
				curr_result[2] = 255;
			}
		}
	}
	cvReleaseImage(&mask_image);
}

//Given an image, it's resized to the same size as the template
//This is used before comparing template and sample
IplImage * resizeToImage(IplImage* source, IplImage * templateSize) {
	IplImage * resizedSample = resizeImage(source, templateSize->width, templateSize->height);
	return resizedSample;
}

//This method is used to determine if the template image
//matches the sample image
Match checkIfImagesMatch(IplImage * sample, IplImage * _template) {
	int width_step = sample->widthStep;
	int pixel_step = sample->widthStep/sample->width;

	int row, col;

	encounteredColours.clear();

	int difference = 0;//The total difference in pixel value between the template and sample
	int matchingPoints = 0;//The number of pixels that match

	cvSetMouseCallback("Hue", on_mouse_show_values, 0);
	window_name_for_on_mouse_show_values = "Hue";
	image_for_on_mouse_show_values = sample;

	// Scan image
	for (row=0; row < sample->height; row++) {
		for (col=0; col < sample->width; col++) {
			unsigned char * sample_point = GETPIXELPTRMACRO(sample, col, row, width_step, pixel_step);//Get current point in image
			unsigned char * template_point = GETPIXELPTRMACRO(_template, col, row, width_step, pixel_step);

			//Sum the total difference across all 3 channels -> this is used to determine match
			difference += abs(sample_point[RED_CH] - template_point[RED_CH]);
			difference += abs(sample_point[GREEN_CH] - template_point[GREEN_CH]);
			difference += abs(sample_point[BLUE_CH] - template_point[BLUE_CH]);

			//If the pixel values are the same
			if(sample_point[0] == template_point[0]) {
				matchingPoints++;
			}

		}
	}

	//Using the total matching points (from the loop above) to determine the overall % of match
	int percentageOfMatches = (matchingPoints * 100) / (sample->height * sample->width);

	//This figure worked well when determining matches. It was obtained through trial and error
	//and the inspection of the matches it was making
	if((difference <= 600000)) {//If overall difference if pixel values less than/equal to amount, create new match

		//If successful match, create new match and return it
		Match newMatch;
		newMatch.difference = difference;
		newMatch.percentageMatch = percentageOfMatches;
		newMatch._template = _template;

		return newMatch;
	}

	//No match
	Match noMatch;
	noMatch._template = NULL;
	return noMatch;
}

//This method loops through the samples and templates and tries to determine
//if there is a match. If there is no match
void templateMatching(vector<ROI> samples, vector<ROI> templates, IplImage * destination) {
	vector<ROI>::iterator samplesIterator, templatesIterator;

	int templateNumber = 1;
	int sampleNumber = 1;

	unsigned int difference = std::numeric_limits<unsigned int>::max() -1;//have to set to something large so it gets set eventually

	//Iterate through all samples and template and determine which match and which don't
	for(samplesIterator = samples.begin(); samplesIterator != samples.end(); ++samplesIterator) {
		for(templatesIterator = templates.begin(); templatesIterator != templates.end(); ++templatesIterator) {
			ROI sampleROI = * samplesIterator;
			ROI templateROI = * templatesIterator;

			//Resize the sample to the same size as the template
			IplImage * resizedSample = resizeToImage(sampleROI.image, templateROI.image);
			IplImage * _template = templateROI.image;

			//Calling method to check if a match has occurred
			Match match = checkIfImagesMatch(resizedSample, _template);

			//If a match HAS occurred
			if(match._template != NULL) {

				//If the current match is a better match than the previous
				if(match.difference < difference) {

					difference = match.difference;//Update difference variable so only best match is used

					//This is where the use of the ROI class is useful
					//Since I saved the original x and y coordinates I can now place them on the image close to the original
					int x = sampleROI.x ;
					int y = sampleROI.y;

					int y_offset = (y+_template->height);//Place the template slightly downwards from the template

					//Overlay the image onto the original
					overlayImage(destination, _template, x, y_offset);
				}
			}
		}

		difference = std::numeric_limits<unsigned int>::max() -1;//Reset difference variable
	}

}

//Main program
int main(int argc, char** argv) {
	int selected_image_num = 1;
	char show_ch = 's';

	IplImage* images[5];
	IplImage* selected_image = NULL;
	IplImage* temp_image = NULL;
	IplImage* red_point_image = NULL;
	IplImage* connected_reds_image = NULL;
	IplImage* connected_background_image = NULL;
	IplImage* result_image = NULL;

	CvSeq * red_components = NULL;
	CvSeq * background_components = NULL;

	// Load in all the images
	if ((images[0] = cvLoadImage(argv[1], -1)) == 0) {
		return 0;
	}

	if ((images[1] = cvLoadImage(argv[2], -1)) == 0) {
		return 0;
	}

	//Template image #1
	if ((images[2] = cvLoadImage(argv[3], -1)) == 0) {
		return 0;
	}

	//Template image #2
	if ((images[3] = cvLoadImage(argv[4], -1)) == 0) {
		return 0;
	}

	//Template image #3
	if ((images[4] = cvLoadImage(argv[5], -1)) == 0) {
		return 0;
	}

	// Explain the User Interface
	printf("Hot keys: \n"
			"\tESC - quit the program\n"
			"\t1 - Real Road Signs (image 1)\n"
			"\t2 - Real Road Signs (image 2)\n"
			"\t3 - Synthetic Road Signs\n"
			"\t4 - Synthetic Parking Road Sign\n"
			"\t5 - Synthetic No Parking Road Sign\n"
			"\tr - Show red points\n"
			"\tc - Show connected red points\n"
			"\th - Show connected holes (non-red points)\n"
			"\ts - Show optimal signs\n");

	// Create display windows for images
	cvNamedWindow("Original", 1);
	cvNamedWindow("Processed Image", 1);
	cvNamedWindow("Matched", 1);

	// Setup mouse call-back on the original image so that the user can see image values as they move the
	// cursor over the image.
	cvSetMouseCallback("Original", on_mouse_show_values, 0);
	window_name_for_on_mouse_show_values = "Original";
	image_for_on_mouse_show_values = selected_image;

	vector<ROI> templateImages;

	//Load templates
	red_point_image = cvCloneImage(images[2]);
	result_image = cvCloneImage(images[2]);
	temp_image = cvCloneImage(images[2]);
	connected_reds_image = cvCloneImage(images[2]);
	connected_background_image = cvCloneImage(images[2]);

	//Obtain the red points of the signs
	find_red_points(images[2], red_point_image, temp_image);
	//Find all connected red components and colour them
	red_components = connected_components(red_point_image, connected_reds_image);
	//Invert the image
	invert_image(red_point_image, temp_image);
	//Find all background connected components (within the sign)
	background_components = connected_components(temp_image, connected_background_image);
	//Optimal sign classification
	determine_optimal_sign_classification(images[2], red_point_image, red_components, background_components, result_image);

	//Setup templates and insert into vector
	vector<ROI> extractedTemplates = extractSign(connected_reds_image, result_image);
	templateImages.insert(templateImages.end(), extractedTemplates.begin(), extractedTemplates.end());

	//Load samples into vector
	vector<ROI> sampleImages;

	int user_clicked_key = 0;
	do {
		//Empty sample image vector as we are processing again
		sampleImages.clear();

		// Create images to do the processing in.
		if (red_point_image != NULL) {
			cvReleaseImage(&red_point_image);
			cvReleaseImage(&temp_image);
			cvReleaseImage(&connected_reds_image);
			cvReleaseImage(&connected_background_image);
			cvReleaseImage(&result_image);
		}

		selected_image = images[selected_image_num - 1];
		red_point_image = cvCloneImage(selected_image);
		result_image = cvCloneImage(selected_image);
		temp_image = cvCloneImage(selected_image);
		connected_reds_image = cvCloneImage(selected_image);
		connected_background_image = cvCloneImage(selected_image);

		// Process image
		image_for_on_mouse_show_values = selected_image;
		find_red_points(selected_image, red_point_image, temp_image);
		red_components = connected_components(red_point_image, connected_reds_image);
		invert_image(red_point_image, temp_image);
		background_components = connected_components(temp_image, connected_background_image);
		determine_optimal_sign_classification(selected_image, red_point_image, red_components, background_components, result_image);

		IplImage * redPointsImage = cvCloneImage(result_image);
		vector<ROI> extractedSamples = extractSign(connected_reds_image, redPointsImage);
		sampleImages.insert(sampleImages.end(), extractedSamples.begin(), extractedSamples.end());

		IplImage * result = cvCloneImage(selected_image);

		//Carry out the template matching
		templateMatching(sampleImages, templateImages, result);
		cvShowImage("Matched", result);

		// Show the original & result
		cvShowImage("Original", selected_image);
		do {
			if ((user_clicked_key == 'r') || (user_clicked_key == 'c')
					|| (user_clicked_key == 'h') || (user_clicked_key == 's'))
				show_ch = user_clicked_key;
			switch (show_ch) {

			case 'c':
				cvShowImage("Processed Image", connected_reds_image);
				break;
			case 'h':
				cvShowImage("Processed Image", connected_background_image);
				break;
			case 'r':
				cvShowImage("Processed Image", red_point_image);
				break;
			case 's':
			default:
				cvShowImage("Processed Image", result_image);
				break;
			}
			user_clicked_key = cvWaitKey(0);

		} while ((!((user_clicked_key >= '1')
				&& (user_clicked_key <= '0' + NUM_IMAGES)))
				&& (user_clicked_key != ESC));
		if ((user_clicked_key >= '1')
				&& (user_clicked_key <= '0' + NUM_IMAGES)) {
			selected_image_num = user_clicked_key - '0';
		}
	} while (user_clicked_key != ESC);

	return 1;
}//end class
