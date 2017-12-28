
#include "stdafx.h"
#include <iostream>
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

using namespace cv;
using namespace std;

int main(int argc, char** argv)
{
	VideoCapture cap(1);

	if (!cap.isOpened())
	{
		cout << "Cannot open the web cam" << endl;
		return -1;
	}

	namedWindow("Control", CV_WINDOW_AUTOSIZE); //create a window called "Control"

	//board limits
	int boardLowH = 150;
	int boardHighH = 179;

	int boardLowS = 40;
	int boardHighS = 200;

	int boardLowV = 75;
	int boardHighV = 255;

	//test values
	int iLowH = 60;
	int iHighH = 90;

	int iLowS = 80;
	int iHighS = 255;

	int iLowV = 190;
	int iHighV = 255;

	//Create trackbars in "Control" window
	cvCreateTrackbar("Low Hue", "Control", &iLowH, 179); //Hue (0 - 179)
	cvCreateTrackbar("High Hue", "Control", &iHighH, 179);

	cvCreateTrackbar("Low Saturation", "Control", &iLowS, 255); //Saturation (0 - 255)
	cvCreateTrackbar("High Saturation", "Control", &iHighS, 255);

	cvCreateTrackbar("Low Value", "Control", &iLowV, 255); //Value (0 - 255)
	cvCreateTrackbar("High Value", "Control", &iHighV, 255);

	while (true)
	{
		Mat imgOriginal;

		bool bSuccess = cap.read(imgOriginal); // read a new frame from video

		if (!bSuccess) //if not success, break loop
		{
			cout << "Cannot read a frame from video stream" << endl;
			break;
		}

		Mat imgHSV;

		cvtColor(imgOriginal, imgHSV, COLOR_BGR2HSV); //Convert the captured frame from BGR to HSV

		Mat boardThreash;
		Mat imgThresholded;

		inRange(imgHSV, Scalar(boardLowH, boardLowS, boardLowV), Scalar(boardHighH, boardHighS, boardHighV), boardThreash);

		//morphological opening (remove small objects from the foreground)
		erode(boardThreash, boardThreash, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
		dilate(boardThreash, boardThreash, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));

		//morphological closing (fill small holes in the foreground)
		dilate(boardThreash, boardThreash, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
		erode(boardThreash, boardThreash, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));

		inRange(imgHSV, Scalar(iLowH, iLowS, iLowV), Scalar(iHighH, iHighS, iHighV), imgThresholded); //Threshold the image
																									 
		//morphological opening (remove small objects from the foreground)
		erode(imgThresholded, imgThresholded, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
		dilate(imgThresholded, imgThresholded, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));

		//morphological closing (fill small holes in the foreground)
		dilate(imgThresholded, imgThresholded, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
		erode(imgThresholded, imgThresholded, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));

		// Find the contours.
		vector<vector<Point>> contours;
		Mat contourOutput = boardThreash.clone();
		findContours(contourOutput, contours, CV_RETR_LIST, CV_CHAIN_APPROX_NONE);

		//Draw the contours       
		Mat contourImage = imgOriginal.clone();
		for (size_t idx = 0; idx < contours.size(); idx++)
		{
			drawContours(contourImage, contours, idx, Scalar(255, 0, 0));
		}

		//create windows
		namedWindow("Original", WINDOW_AUTOSIZE);
		namedWindow("Thresholded Image", WINDOW_AUTOSIZE);
		namedWindow("Contours", WINDOW_AUTOSIZE);

		//move windows
		moveWindow("Original", 50, 50);
		moveWindow("Thresholded Image", 690, 50);
		moveWindow("Contours", 1230, 50);

		//show videos in windows
		imshow("Original", imgOriginal);
		imshow("Thresholded Image", imgThresholded);
		imshow("Contours", contourImage);

		//'esc' key pressed for 30ms
		if (waitKey(30) == 27) {
			cout << "esc key is pressed by user" << endl;
			break;
		}
	}
	return 0;
}