#ifdef _CH_
#pragma package <opencv>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "cv.h"
#include "highgui.h"
#include "../utilities.h"

using namespace std;

#define VARIATION_ALLOWED_IN_PIXEL_VALUES 30
#define ALLOWED_MOTION_FOR_MOTION_FREE_IMAGE 1.0
#define NUMBER_OF_POSTBOXES 6
#define MINIMUM_GRADIENT_VALUE 5
int PostboxLocations[NUMBER_OF_POSTBOXES][5] = {
                                {   6,  73,  95, 5, 92 }, {   6,  73,  95, 105, 192 },
                                { 105, 158, 193, 5, 92 }, { 105, 158, 193, 105, 192 },
                                { 204, 245, 292, 5, 92 }, { 204, 245, 292, 105, 192 } };

#define POSTBOX_TOP_ROW 0
#define POSTBOX_TOP_BASE_ROW 1
#define POSTBOX_BOTTOM_ROW 2
#define POSTBOX_LEFT_COLUMN 3
#define POSTBOX_RIGHT_COLUMN 4

void convertToGrayscale(IplImage * source, IplImage * result) {
	cvCvtColor(source, result, CV_RGB2GRAY);
}

void sobel(IplImage * source, IplImage * result) {
	int matrixSize = 3;

	float conv[3][3] = {{-1, 0, 1},
					  	{-2, 0, 2},
					  	{-1, 0, 1}};

	CvMat kernel = cvMat(matrixSize, matrixSize, CV_32FC1, conv);
	CvPoint anchor = cvPoint(-1, -1);

	cvFilter2D(source, result, &kernel, anchor);
}

void sobel2(IplImage * source, IplImage * result) {
	cvZero(result);

	//Sobel convolution matrix
	int gx[] = {-1, 0, 1,
				-2, 0, 2,
				-1, 0, 1};

	int gy[] = {-1, -2, -1,
				 0, 0, 0,
				 1, 2, 1};

	//use absolute or square
	int convolutions = 9;//size of convolution matrices

	int width_step = source->widthStep;
	int pixel_step = source->widthStep / source->width;

	int result_width_step = result->widthStep;
	int result_pixel_step = result->widthStep / result->width;
	int result_channels = result->nChannels;

	unsigned char white[4] = {255,255,255,0};
	unsigned char black[4] = {0, 0, 0, 0};

	int convX = 0, convY = 0;
	//Scan image and determine background and object pixels
	for (int row = 1; row < (source->height-1); row++) {
		for (int col = 1; col < (source->width-1); col++) {

			unsigned char * top_left = GETPIXELPTRMACRO(source, (col-1), (row-1), width_step, pixel_step);
			unsigned char * top = GETPIXELPTRMACRO(source, col, (row-1), width_step, pixel_step);
			unsigned char * top_right = GETPIXELPTRMACRO(source, (col+1), (row-1), width_step, pixel_step);

			unsigned char * left = GETPIXELPTRMACRO(source, (col-1), row, width_step, pixel_step);
			unsigned char * right = GETPIXELPTRMACRO(source, (col+1), row, width_step, pixel_step);

			unsigned char * bottom_left = GETPIXELPTRMACRO(source, (col-1), (row+1), width_step, pixel_step);
			unsigned char * bottom = GETPIXELPTRMACRO(source, col, (row+1), width_step, pixel_step);
			unsigned char * bottom_right = GETPIXELPTRMACRO(source, (col+1), (row+1), width_step, pixel_step);

			unsigned char * positions[] = {top_left, top, top_right, left, 0, right , bottom_left, bottom, bottom_right};

			for(int i=0;i<convolutions;i++) {
				convX += (gx[i] * positions[0][i]);
				convY += (gy[i] * positions[0][i]);
			}

			int gradientMagnitude = abs(convX + convY);

			if(convX != 0) {
				int orientationAngle = atan(convY/convX);// 0 -> the direction of maximum contrast from black to white runs from left to right on the image
				//Store this for use later in tracing algorithm
			}

			//Fix offset
			//convX += 1024;
			//convX /= 4;

			//Only care about vertical lines
			if(convX <= 0) {//was 255 without maxima
				convX = abs(convX);
				unsigned char covergence[4] = {convX, convX, convX, 0};

				//cout << "ConvX = " << convX << endl;
				PUTPIXELMACRO(result, col, row, covergence, result_width_step, result_pixel_step, result_channels);//or just use black
			} else if(convX >= 255) {
				PUTPIXELMACRO(result, col, row, white, result_width_step, result_pixel_step, result_channels);
			}

			convX = 0;//Reset for next iteration
			convY = 0;
		}
	}

}

//Computer non suppressed maximum of image (only along the rows)
void nonSuppressedMaxima(IplImage * source) {
	int width_step = source->widthStep;
	int pixel_step = source->widthStep / source->width;
	int channels = source->nChannels;

	unsigned char black[4] = {0, 0, 0, 0};

	for (int row = 0; row < source->height; row++) {
		for (int col = 1; col < (source->width-1); col++) {
			unsigned char * left = GETPIXELPTRMACRO(source, (col-1), row, width_step, pixel_step);
			unsigned char * right = GETPIXELPTRMACRO(source, (col+1), row, width_step, pixel_step);

			unsigned char * curr_point = GETPIXELPTRMACRO(source, col, row, width_step, pixel_step);

			if((curr_point[0] < left[0]) || (curr_point[0] <= right[0])) {
				PUTPIXELMACRO(source, col, row, black, width_step, pixel_step, channels);
			}
		}
	}
}

void indicate_post_in_box(IplImage* image, int postbox) {
	write_text_on_image(image,(PostboxLocations[postbox][POSTBOX_TOP_ROW]+PostboxLocations[postbox][POSTBOX_BOTTOM_ROW])/2,PostboxLocations[postbox][POSTBOX_LEFT_COLUMN]+2, "Post in");
	write_text_on_image(image,(PostboxLocations[postbox][POSTBOX_TOP_ROW]+PostboxLocations[postbox][POSTBOX_BOTTOM_ROW])/2+19,PostboxLocations[postbox][POSTBOX_LEFT_COLUMN]+2, "this box");
}

//Tracing algorithm
//Given an edge image where every pixel xi has an associated gradient s(xi) and orientation ɸ(xi) - edge direction
//We have saved orientation from computing sobel with gx and gy
void graphCreation(IplImage * source, int threshold) {

	int width_step = source->widthStep;
	int pixel_step = source->widthStep / source->width;

	int number_of_nodes = 0;

	//Scan image and determine background and object pixels
	for (int row = 0; row < source->height; row++) {
		for (int col = 0; col < source->width; col++) {
			unsigned char * curr_point = GETPIXELPTRMACRO(source, col, row, width_step, pixel_step);

			if(curr_point[0] > threshold) {
				//Create node

				for(int i=0;i<number_of_nodes;i++) {

				}

			}
		}
	}

}

void scanPostboxes(IplImage * last_motion_free_frame, IplImage * source, IplImage * labelled_output_image) {
	int width_step = source->widthStep;
	int pixel_step = source->widthStep / source->width;
	int channels = source->nChannels;

	unsigned char colour[4] = {128,0,128,0};

	int white_lines_hit = 0;

	//For each post box..
	for(int i=0;i<NUMBER_OF_POSTBOXES;i++) {
		//int startRow = PostboxLocations[i][POSTBOX_TOP_BASE_ROW];
		int endRow = PostboxLocations[i][POSTBOX_BOTTOM_ROW];
		int startColumn = PostboxLocations[i][POSTBOX_LEFT_COLUMN];
		int endColumn = PostboxLocations[i][POSTBOX_RIGHT_COLUMN];

		//Determine horizontally how many white lines (vertical edges) are hit
		//Could expand this to do more than one horizontal line!
		for (int row = endRow-3; row < endRow-2; row++) {
			for (int col = startColumn; col < endColumn; col++) {
				//PUTPIXELMACRO(source, col, row, colour, width_step, pixel_step, channels);//Show area covered
				unsigned char * currentFrame = GETPIXELPTRMACRO(source, col, row, width_step, pixel_step);

				if(currentFrame[0] > 250) {
					white_lines_hit++;
				}
			}
		}

		//if there only exists < 4 vertical edges - we will assume there is post in that post box
		if(white_lines_hit < 4) {
			int postbox = i;
			//labelled_output_image = cvCloneImage(last_motion_free_frame);
			indicate_post_in_box(last_motion_free_frame, postbox);
			cvShowImage( "Results", last_motion_free_frame);
		}

		//cout << "White points = " << white_lines_hit << " for postbox #" << i << endl;
		white_lines_hit = 0;
	}
}

// TO-DO:  Compute the partial first derivative edge image in order to locate the vertical edges in the passed image,
	//   and then determine the non-maxima suppressed version of these edges (along each row as the rows can be treated
	//   independently as we are only considering vertical edges). Output the non-maxima suppressed edge image.
	// Note:   You may need to smooth the image first.
void compute_vertical_edge_image(IplImage* input_image, IplImage* output_image) {
	IplImage * greyscale = cvCreateImage(cvGetSize(input_image), IPL_DEPTH_8U, 1);
	cvZero(greyscale);
	convertToGrayscale(input_image, greyscale);

	sobel(greyscale, output_image);//Does smoothing too
	nonSuppressedMaxima(output_image);//Makes lines only 1-pixel thick
}

// TO-DO:  Determine the percentage of the pixels which have changed (by more than VARIATION_ALLOWED_IN_PIXEL_VALUES)
//        and return whether that percentage is less than ALLOWED_MOTION_FOR_MOTION_FREE_IMAGE.
bool motion_free_frame(IplImage* current_frame, IplImage* previous_frame) {

	int current_width_step = current_frame->widthStep;
	int current_pixel_step = current_frame->widthStep / current_frame->width;

	int previous_width_step = previous_frame->widthStep;
	int previous_pixel_step = previous_frame->widthStep / previous_frame->width;

	float totalPixelsPerFrame = current_frame->width * current_frame->height;

	float numberOfPixelsChanged = 0;

	for (int row = 0; row < current_frame->height; row++) {
		for (int col = 0; col < current_frame->width; col++) {
			unsigned char * currentFrame = GETPIXELPTRMACRO(current_frame, col, row, current_width_step, current_pixel_step);
			unsigned char * previousFrame = GETPIXELPTRMACRO(previous_frame, col, row, previous_width_step, previous_pixel_step);

			int difference = currentFrame[0] - previousFrame[0];//Calculate difference in intensity between previous and current frame

			if(abs(difference) >= VARIATION_ALLOWED_IN_PIXEL_VALUES) {
				numberOfPixelsChanged++;
			}
		}
	}

	//Calculate percentage of pixels changed
	double percentageChange = (numberOfPixelsChanged * 100.00);
	percentageChange = percentageChange / totalPixelsPerFrame;

	return percentageChange < ALLOWED_MOTION_FOR_MOTION_FREE_IMAGE;
}

// TO-DO:  If the input_image is not motion free then do nothing.  Otherwise determine the vertical_edge_image and check
//        each postbox to see if there is mail (by analysing the vertical edges).  Highlight the edge points used during your
//        processing.  If there is post in a box indicate that there is on the labelled_output_image.
void check_postboxes(IplImage* input_image, IplImage * previous_frame, IplImage* labelled_output_image, IplImage* vertical_edge_image ) {

	if(motion_free_frame(input_image, previous_frame)){
		compute_vertical_edge_image(input_image, vertical_edge_image);
		scanPostboxes(input_image, vertical_edge_image, labelled_output_image);

		//instead of doing horizontal lines
		//Now analyze the vertical edges
		//int threshold = 127;
		//graphCreation(vertical_edge_image, threshold);//Highlight the edge points

	} else {
		cout << "MOTION DETECTED" << endl;
	}
}

int main( int argc, char** argv ) {
    IplImage *current_frame=NULL;
	CvSize size;
	size.height = 300; size.width = 200;
	IplImage *corrected_frame = cvCreateImage( size, IPL_DEPTH_8U, 3 );
	IplImage *labelled_image=NULL;
	IplImage *vertical_edge_image=NULL;
	IplImage * previousFrame = NULL;
    int user_clicked_key=0;

    // Load the video (AVI) file
    CvCapture *capture = cvCaptureFromAVI("/Users/wbrowne/Desktop/College/C++Workspace/PostboxStatus/Debug/Postboxes.mp4");

    // Ensure AVI opened properly
    if(!capture) {
		return 1;
    }

    // Get Frames Per Second in order to playback the video at the correct speed
    int fps = (int) cvGetCaptureProperty(capture, CV_CAP_PROP_FPS);

	// Explain the User Interface
    printf( "Hot keys: \n"
		    "\tESC - quit the program\n"
            "\tSPACE - pause/resume the video\n");

	CvPoint2D32f from_points[4] = { {3, 6}, {221, 11}, {206, 368}, {18, 373} };
	CvPoint2D32f to_points[4] = { {0, 0}, {200, 0}, {200, 300}, {0, 300} };
	CvMat* warp_matrix = cvCreateMat( 3,3,CV_32FC1 );
	cvGetPerspectiveTransform(from_points, to_points, warp_matrix);

	// Create display windows for images
	cvNamedWindow("Input video", 0);
	cvNamedWindow("Vertical edges", 0);
    cvNamedWindow("Results", 0);

	// Setup mouse callback on the original image so that the user can see image values as they move the
	// cursor over the image.
    cvSetMouseCallback( "Input video", on_mouse_show_values, 0 );
	window_name_for_on_mouse_show_values="Input video";

    while(user_clicked_key != ESC) {
		// Get current video frame
        current_frame = cvQueryFrame(capture);
		image_for_on_mouse_show_values=current_frame; // Assign image for mouse callback

		if(!current_frame) {// No new frame available
			break;
        }

		cvWarpPerspective(current_frame, corrected_frame, warp_matrix);

		if (labelled_image == NULL) {	// The first time around the loop create the image for processing
			labelled_image = cvCloneImage(corrected_frame);
			//vertical_edge_image = cvCloneImage( corrected_frame );
			vertical_edge_image = cvCreateImage(cvGetSize(corrected_frame), IPL_DEPTH_8U, 1);
			cvZero(vertical_edge_image);

			//previousFrame = cvCreateImage(cvGetSize(corrected_frame), IPL_DEPTH_8U, 1);
			previousFrame = cvCloneImage(corrected_frame);
			cvZero(previousFrame);
		}

		check_postboxes(corrected_frame, previousFrame, labelled_image, vertical_edge_image);

		// Display the current frame and results of processing
        cvShowImage( "Input video", current_frame );
        cvShowImage( "Vertical edges", vertical_edge_image );

        // Wait for the delay between frames
        user_clicked_key = cvWaitKey( 1000 / fps );
		if (user_clicked_key == ' ') {
			user_clicked_key = cvWaitKey(0);
		}
		previousFrame = cvCloneImage(corrected_frame);
	}//end main loop

    /* free memory */
    cvReleaseCapture(&capture);
    cvDestroyWindow("video");

    return 0;
}
