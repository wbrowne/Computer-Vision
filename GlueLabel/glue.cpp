#ifdef _CH_
#pragma package <opencv>
#endif

#include "cv.h"
#include "highgui.h"
#include <stdio.h>
#include <stdlib.h>
#include "../utilities.h"

using namespace std;

#define NUM_IMAGES 9
#define FIRST_LABEL_ROW_TO_CHECK 390
#define LAST_LABEL_ROW_TO_CHECK 490
#define ROW_STEP_FOR_LABEL_CHECK 20

void convertToGrayscale(IplImage * source, IplImage * result) {
	cvCvtColor(source, result, CV_RGB2GRAY);
}

// TO-DO:  Search for the sides of the labels from both the left and right on "row".  The side of the label is taken
//        taken to be the second edge located on that row (the side of the bottle being the first edge).  If the label
//        are found set the left_label_column and the right_label_column and return true.  Otherwise return false.
//        The routine should mark the points searched (in yellow), the edges of the bottle (in blue) and the edges of the
//        label (in red) - all in the result_image.
bool find_label_edges(IplImage* edge_image, IplImage* result_image, int row, int& left_label_column, int& right_label_column) {

	int result_width_step = result_image->widthStep;
	int result_pixel_step = result_image->widthStep / result_image->width;
	int channels = result_image->nChannels;

	unsigned char yellow[4] = {50,240,240,0};
	unsigned char blue[4] = {255,0,0,0};
	unsigned char red[4] = {0,0,255,0};

	bool leftLabelFound = false;
	bool rightLabelFound = false;

	int edges_hit = 0;

	//Loop through columns and determine where the edges are hit (from right to left)
	for (int col = 0; col < result_image->width; col++) {
		unsigned char * curr_point = GETPIXELPTRMACRO(result_image, col, row, result_width_step, result_pixel_step);

		if(curr_point[0] == 255) {//If white point is encountered - must be bottle
			PUTPIXELMACRO(result_image, col, row, blue, result_width_step, result_pixel_step, channels);
			edges_hit++;
		} else {//Place yellow because still searching
			PUTPIXELMACRO(result_image, col, row, yellow, result_width_step, result_pixel_step, channels);
		}

		if(edges_hit == 2) {//The second edge located on that row is taken taken to be the side of the label
			//PUTPIXELMACRO(edge_image, col, row, yellow, edge_width_step, edge_pixel_step, edge_channels);
			PUTPIXELMACRO(result_image, col, row, red, result_width_step, result_pixel_step, channels);
			left_label_column = col;
			leftLabelFound = true;
			break;
		}
	}

	edges_hit = 0;//reuse variable for next loop

	//Only process right side if left label side found
	if(leftLabelFound) {
		//From right to left
		for (int col = result_image->width; col >= result_image->width/2; col--) {
				unsigned char * curr_point = GETPIXELPTRMACRO(result_image, col, row, result_width_step, result_pixel_step);

				if(curr_point[0] == 255) {//If white point is encountered - must be bottle
					edges_hit++;
					PUTPIXELMACRO(result_image, col, row, blue, result_width_step, result_pixel_step, channels);
				} else {//Place yellow because still searching
					PUTPIXELMACRO(result_image, col, row, yellow, result_width_step, result_pixel_step, channels);
				}

				if(edges_hit == 2) {//The second edge located on that row is taken taken to be the side of the label
					PUTPIXELMACRO(result_image, col, row, red, result_width_step, result_pixel_step, channels);
					right_label_column = col;
					rightLabelFound = true;
					break;
				}
		}
	}
	return (leftLabelFound && rightLabelFound);//Return if both left and right label edges found
}

// TO-DO:  Inspect the image of the glue bottle passed.  This routine should check a number of rows as specified by
//        FIRST_LABEL_ROW_TO_CHECK, LAST_LABEL_ROW_TO_CHECK and ROW_STEP_FOR_LABEL_CHECK.  If any of these searches
//        fail then "No Label" should be written on the result image.  Otherwise if all left and right column values
//        are roughly the same "Label Present" should be written on the result image.  Otherwise "Label crooked" should
//        be written on the result image.

//         To implement this you may need to use smoothing (cv::GaussianBlur() perhaps) and edge detection (cvCanny() perhaps).
//        You might also need cvConvertImage() which converts between different types of image.
void check_glue_bottle(IplImage* original_image, IplImage* result_image) {
	IplImage * greyscale = cvCreateImage(cvGetSize(original_image), IPL_DEPTH_8U, 1);
	cvZero(greyscale);
	convertToGrayscale(original_image, greyscale);//Convert to greyscale

	cvSmooth(greyscale, greyscale, CV_GAUSSIAN, 5, 5);//Gaussian smoothing

	cvAddS(greyscale, cvScalar(80,80,80), greyscale);//Adjust brightness
	//cvScale(greyscale, greyscale, 1.0);//contrast
	//cvEqualizeHist(greyscale, greyscale);//Equalize image

	// Perform Canny edge detection
	IplImage * edge_image = cvCreateImage(cvGetSize(original_image), 8, 1);
	cvConvertImage(original_image, edge_image);
	cvCanny(greyscale, edge_image, 10, 125);

	int l_columns[6], r_columns[6];
	bool straight, nolabel = false;

	cvConvertImage(edge_image, result_image);

	int index = 0;
	//Loop through image rows
	for (int row = FIRST_LABEL_ROW_TO_CHECK; row <= LAST_LABEL_ROW_TO_CHECK; row+=ROW_STEP_FOR_LABEL_CHECK) {

		int left_label_column = 0, right_label_column = 0;
		//Determine the label edges
		bool labelEdgesFound = find_label_edges(edge_image, result_image, row, left_label_column, right_label_column);

		//Store the label edges (columns)
		if(labelEdgesFound) {
			l_columns[index] = left_label_column;
			r_columns[index] = right_label_column;
		} else {
			nolabel = true;
			break;
		}
		index++;
	}

	int delta = 1;//Variable used for column difference
	straight = true;//Assume label is straight
	if(!nolabel) {//If a label was found

		//Now determine if straight or crooked
		for(int i=0;i<3;i++) {
			if(abs(l_columns[i] - l_columns[i+1]) > delta) {
				//left crooked
				straight = false;
			}

			if(abs(r_columns[i] - r_columns[i+1]) > delta) {
				//right crooked
				straight = false;
			}
		}

		if(straight) {
			write_text_on_image(result_image, 20, 20, "Straight label");
		} else {
			write_text_on_image(result_image, 20, 20, "Crooked label");
		}
	} else {
		write_text_on_image(result_image, 20, 20, "No label");
	}
}

int main(int argc, char** argv) {
	int selected_image_num = 1;
	IplImage* selected_image = NULL;
	IplImage* images[NUM_IMAGES];
	IplImage* result_image = NULL;

	// Load all the images.
	for (int file_num=1; (file_num <= NUM_IMAGES); file_num++) {
		char filename[100];
		sprintf(filename, "/Users/wbrowne/Desktop/College/C++Workspace/GlueBottles/Glue%d.jpg", file_num);
		if((images[file_num-1] = cvLoadImage(filename,-1)) == 0) {
			return 0;
		}
	}

	// Explain the User Interface
    printf( "Hot keys: \n"
            "\tESC - quit the program\n");
    printf( "\t1..%d - select image\n",NUM_IMAGES);

	// Create display windows for images
    cvNamedWindow("Original", 1);
    cvNamedWindow("Processed Image", 1);

	// Create images to do the processing in.
	selected_image = cvCloneImage(images[selected_image_num-1]);
    result_image = cvCloneImage(selected_image);

	// Setup mouse callback on the original image so that the user can see image values as they move the
	// cursor over the image.
    cvSetMouseCallback("Original", on_mouse_show_values, 0);
	window_name_for_on_mouse_show_values="Original";
	image_for_on_mouse_show_values=selected_image;

	int user_clicked_key = 0;
	do {
		// Process image (i.e. setup and find the number of spoons)
		cvCopyImage(images[selected_image_num-1], selected_image);
        cvShowImage("Original", selected_image);
		image_for_on_mouse_show_values=selected_image;
		check_glue_bottle(selected_image, result_image);
		cvShowImage("Processed Image", result_image);

		// Wait for user input
        user_clicked_key = cvWaitKey(0);
		if ((user_clicked_key >= '1') && (user_clicked_key <= '0'+NUM_IMAGES)) {
			selected_image_num = user_clicked_key-'0';
		}
	} while (user_clicked_key != ESC);

    return 1;
}
