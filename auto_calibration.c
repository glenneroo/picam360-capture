#include "auto_calibration.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <stdbool.h>

#include <opencv/cv.h>

void auto_calibration(PICAM360CAPTURE_T *state, FRAME_T *frame) {

	int margin = 32;
	int width = frame->width + 2 * margin;
	int height = frame->height + 2 * margin;

	CvMemStorage* storage = cvCreateMemStorage(0);
	CvSeq* contour = NULL;

	IplImage *image_bin = cvCreateImage(cvSize(width, height),
			IPL_DEPTH_8U, 1);


	//binalize
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			unsigned char val = 0;

			int _x = x - margin;
			int _y = y - margin;
			if (_x >= 0 && _x < frame->width && _y >= 0 && _y < frame->height) {
				val = (frame->img_buff + frame->width * 3 * _y)[_x * 3];
			}

			val = (val >= 32) ? 255 : 0;

			(image_bin->imageData + image_bin->widthStep * y)[x] = val;
		}
	}

	//find countor
	//cvDilate(image_bin, image_bin, 0, 1);
	//cvErode(image_bin, image_bin, 0, 1);
	cvFindContours(image_bin, storage, &contour, sizeof(CvContour),
			CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE, cvPoint(0, 0));

	// loop over all contours
	{
		CvBox2D box = { };
		//CvPoint2D32f box_point[4];
		CvBox2D tmp;
		double max_val = INT_MIN;
		CvSeq *cp = contour;
		while (cp != NULL) {
			double size;
			tmp = cvMinAreaRect2(cp, storage);
			size = tmp.size.width * tmp.size.height;
			if (size > max_val) {
				box = tmp;
				max_val = size;
			}
			cp = cp->h_next;
		}
		printf("%lf,%lf\n", box.center.x, box.center.y);
	}

	//Release
	if (storage != NULL) {
		cvReleaseMemStorage(&storage);
	}
	if (image_bin != NULL) {
		cvReleaseImage(&image_bin);
	}

}
