
#include "stdafx.h"
#include <iostream>
#include <algorithm>
#include "opencv2/core/core.hpp"
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

	//create windows
	namedWindow("Original", WINDOW_AUTOSIZE);
	namedWindow("Thresholded Image", WINDOW_AUTOSIZE);
	namedWindow("Contours", WINDOW_AUTOSIZE);
	namedWindow("Info display", CV_WINDOW_AUTOSIZE);

	//move windows
	moveWindow("Original", 50, 50);
	moveWindow("Thresholded Image", 690, 50);
	moveWindow("Contours", 1230, 50);
	moveWindow("Info display", 50, 600);

	//////////////////////////////////////////
	// engine stuff

	int map[18] = { 0,1,3,6,10,23,35,46,56,65,75,86,98,111,115,118,120,121 };

	Mat boardImage, srcImage, srcImage_gray;

	/// Read the imag
	srcImage = imread("board.png", IMREAD_UNCHANGED);

	if (srcImage.empty())
	{
		// NOT SUCCESSFUL : the data attribute is empty

		cout << "Image file could not be open !!" << std::endl;

		return -1;
	}

	/// Convert it to gray
	cvtColor(srcImage, srcImage_gray, CV_BGR2GRAY);

	/// Reduce the noise so we avoid false circle detection
	GaussianBlur(srcImage_gray, srcImage_gray, Size(9, 9), 2, 2);

	//test values
	int dist = 20;

	int threshHigh = 30;
	int threshCenter = 12;

	int minRad = 7;
	int maxRad = 10;

	int board[121];

	//////////////////////////////////////////////////////////
	//testing of display - TODO: Use info gathered from real life here

	//initialize everything with 0
	for (int i = 0; i < 17; i++) {
		for (int j = map[i]; j < map[i+1]; j++) {
			board[j] = 0;
		}
	}

	//fill in the top triangle with marbles
	for (int i = 0; i < 4; i++) {
		for (int j = map[i]; j < map[i + 1]; j++) {
			board[j] = 1;
		}
	}

	//fill in the bottom triangle with marbles
	for (int i = 13; i < 17; i++) {
		for (int j = map[i]; j < map[i + 1]; j++) {
			board[j] = 2;
		}
	}
	/////////////////////////////////////////////////////////

	//DEBUG print memory representation of map
	/* 
	for (int i = 0; i < 17; i++) {
		for (int j = map[i]; j < map[i + 1]; j++) {
			cout << board[j];
		}
		cout << endl;
	}
	*/


	vector<Vec3f> circles;

	/// Apply the Hough Transform to find the circles
	HoughCircles(srcImage_gray, circles, CV_HOUGH_GRADIENT, 1, srcImage_gray.rows / dist, threshHigh, threshCenter, minRad, maxRad);

	sort(circles.begin(), circles.end(),
		[](const Vec3f& a, const Vec3f& b) {

		if (a[1] + 5 < b[1]) {
			return true;
		}
		else if (a[1] - 5 > b[1]) {
			return false;
		}
		else { //same row
			if (a[0] < b[0]) {
				return true;
			}
			else {
				return false;
			}
		}
	});
	

	//////////////////////////////////////////

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

		//////////////////////////////////////////////////////////////////////
		// Engine

		Mat srcImage_circles = srcImage.clone();

		/// Draw the circles detected
		for (size_t i = 0; i < circles.size(); i++)
		{
			Point center(cvRound(circles[i][0]), cvRound(circles[i][1]));

			switch (board[i]) {
			case 1: //blue
				circle(srcImage_circles, center, 11, Scalar(197, 97, 33), -1, 8, 0);
				break;
			case 2:	//green
				circle(srcImage_circles, center, 11, Scalar(128, 163, 99), -1, 8, 0);
				break;
			case 3:	//white
				circle(srcImage_circles, center, 11, Scalar(242, 232, 222), -1, 8, 0);
				break;
			case 4:	//orange
				circle(srcImage_circles, center, 11, Scalar(0, 138, 239), -1, 8, 0);
				break;
			case 5:	//yellow
				circle(srcImage_circles, center, 11, Scalar(62, 205, 232), -1, 8, 0);
				break;
			case 6:	//black
				circle(srcImage_circles, center, 11, Scalar(19, 18, 22), -1, 8, 0);
				break;
			}
		}

		/// Show your results
		imshow("Info display", srcImage_circles);

		//////////////////////////////////////////////////////////////////////

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