#include "stdafx.h"

#include "opencv2/core.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/aruco.hpp"
#include "opencv2/calib3d.hpp"

#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>

using namespace std;
using namespace cv;

const float calibrationSquareDimension = 0.026f;
const float arucoSquareDimension = 0.030f;
const Size chessBoardDimensions = Size(9, 6);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//used to create and print out aruco markers
void createArucoMarkers()
{
	Mat outputMarker;

	Ptr<aruco::Dictionary> markerDictionary = aruco::getPredefinedDictionary(aruco::PREDEFINED_DICTIONARY_NAME::DICT_4X4_50);

	for (int i = 0; i < 50; i++)
	{
		aruco::drawMarker(markerDictionary, i, 500, outputMarker, 1);
		ostringstream convert;
		string imageName = "4x4Marker_";
		convert << imageName << i << ".png";
		imwrite(convert.str(), outputMarker);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//create ideal world positions on chess board
void createKnownBoardPositions(Size boardSize, float squareEdgeLength, vector<Point3f>& corners)
{
	for (int i = 0; i < boardSize.height; i++)
	{
		for (int j = 0; j < boardSize.width; j++)
		{
			corners.push_back(Point3f(j * squareEdgeLength, i * squareEdgeLength, 0.0f));
		}
	}
}

//find the realworld chess board corners in a set of images
void getChessBoardCorners(vector<Mat> images, vector<vector<Point2f>>& allFoundCorners, bool showResults = false)
{
	for (vector<Mat>::iterator iter = images.begin(); iter != images.end(); iter++)
	{
		vector<Point2f> pointBuf;
		bool found = findChessboardCorners(*iter, Size(9, 6), pointBuf, CV_CALIB_CB_ADAPTIVE_THRESH | CV_CALIB_CB_NORMALIZE_IMAGE);

		if (found)
		{
			allFoundCorners.push_back(pointBuf);
		}

		if (showResults)
		{
			drawChessboardCorners(*iter, Size(9, 6), pointBuf, found);
			imshow("Looking for Corners", *iter);
			waitKey(0);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//finds camera calibration values using a set of images
void cameraCalibration(vector<Mat> calibrationImages, Size boardSize, float squareEdgeLength, Mat& cameraMatrix, Mat& distCoefficients)
{
	vector<vector<Point2f>> chessBoardImageSpacePoints;
	getChessBoardCorners(calibrationImages, chessBoardImageSpacePoints, false);

	vector<vector<Point3f>> worldSpaceCornerPoints(1);

	createKnownBoardPositions(boardSize, squareEdgeLength, worldSpaceCornerPoints[0]);
	worldSpaceCornerPoints.resize(chessBoardImageSpacePoints.size(), worldSpaceCornerPoints[0]);

	vector<Mat> rVectors, tVectors;
	distCoefficients = Mat::zeros(8, 1, CV_64F);

	calibrateCamera(worldSpaceCornerPoints, chessBoardImageSpacePoints, boardSize, cameraMatrix, distCoefficients, rVectors, tVectors);
}

//print out a file with the camera calibration values
bool saveCamCalib(string name, Mat cameraMatrix, Mat distCoefficients)
{
	ofstream outStream(name);
	if (outStream)
	{
		uint16_t rows = cameraMatrix.rows;
		uint16_t cols = cameraMatrix.cols;

		outStream << rows << endl;
		outStream << cols << endl;

		for (int r = 0; r < rows; r++)
		{
			for (int c = 0; c < cols; c++)
			{
				double value = cameraMatrix.at<double>(r, c);
				outStream << value << endl;
			}
		}
		rows = distCoefficients.rows;
		cols = distCoefficients.cols;

		outStream << rows << endl;
		outStream << cols << endl;

		for (int r = 0; r < rows; r++)
		{
			for (int c = 0; c < cols; c++)
			{
				double value = distCoefficients.at<double>(r, c);
				outStream << value << endl;
			}
		}
		outStream.close();
		return true;
	}
	return false;
}

//CALL THIS to calibrate a webcam
//space key takes snapshots (get > 15), enter key tries to find calibration for webcam and print a file with the values
int calibWebcam(Mat& cameraMatrix, Mat& distCoefficients)
{
	Mat frame;
	Mat drawToFrame;

	vector<Mat> savedImages;

	vector< vector<Point2f> > markerCorners, rejectedCandidates;

	VideoCapture vid(0);

	if (!vid.isOpened())
	{
		cout << "Cannot open the web cam!" << endl;
		return -1;
	}

	int fps = 20;

	namedWindow("Webcam", CV_WINDOW_AUTOSIZE);
	namedWindow("Webcam", CV_WINDOW_AUTOSIZE);
	namedWindow("Webcam", CV_WINDOW_AUTOSIZE);
	namedWindow("Webcam", CV_WINDOW_AUTOSIZE);

	while (true)
	{
		bool bSuccess = vid.read(frame);

		if (!bSuccess)
		{
			cout << "Cannot read a frame from video stream" << endl;
			break;
		}

		Mat foundPoints;
		bool found = false;

		found = findChessboardCorners(frame, Size(9, 6), foundPoints, CV_CALIB_CB_ADAPTIVE_THRESH | CV_CALIB_CB_NORMALIZE_IMAGE);
		frame.copyTo(drawToFrame);
		drawChessboardCorners(drawToFrame, Size(9, 6), foundPoints, found);
		if (found)
			imshow("Webcam", drawToFrame);
		else
			imshow("Webcam", frame);

		char c = waitKey(1000 / fps);

		switch (c)
		{
		case ' ':
			//save the image
			if (found)
			{
				Mat temp;
				frame.copyTo(temp);
				savedImages.push_back(temp);
			}
			break;
		case 13: //enter key
				 //start calibration
			if (savedImages.size() > 15)
			{
				cameraCalibration(savedImages, chessBoardDimensions, calibrationSquareDimension, cameraMatrix, distCoefficients);
				saveCamCalib("CamCalibValues", cameraMatrix, distCoefficients);
			}
			break;
		case 27:
			//exit
			return 0;
			break;
		}
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//find an aruco marker (needs camera calibration values)
int startWebcamMonitoring(const Mat& cameraMatrix, const Mat& distCoefficients, float arucoSquareDimension)
{
	Mat frame;
	Mat warpedDisplay;

	vector<int> markerIds;
	vector<vector<Point2f>> markerCorners, rejectedCandidates;
	Ptr<aruco::DetectorParameters> parameters = aruco::DetectorParameters::create();

	Ptr<aruco::Dictionary> markerDict = aruco::getPredefinedDictionary(aruco::PREDEFINED_DICTIONARY_NAME::DICT_4X4_50);

	VideoCapture vid(0);

	if (!vid.isOpened())
	{
		cout << "Cannot open the web cam!" << endl;
		return -1;
	}

	namedWindow("Webcam", CV_WINDOW_AUTOSIZE);
	namedWindow("Warped Display", CV_WINDOW_AUTOSIZE);
	namedWindow("Info display", CV_WINDOW_AUTOSIZE);

	moveWindow("Webcam", 1200, 2);
	moveWindow("Warped Display", 1200, 500);
	moveWindow("Info display", 720, 300);

	vector<Vec3d> rotationVectors, translationVectors;
	Mat rotationMat;

	//display
	Mat srcImage, srcImage_gray, warning;

	// Read the imag
	srcImage = imread("board.png", IMREAD_UNCHANGED);
	warning = imread("noSight.png", IMREAD_UNCHANGED);

	//uncomment for board2.png
	//int x = 450;
	//int y = (srcImage.rows * 450) / srcImage.cols;
	//Size size(x, y);
	//resize(srcImage, srcImage, size);

	if (srcImage.empty()) {
		// NOT SUCCESSFUL : the data attribute is empty
		cout << "Image file could not be open !!" << std::endl;
		return -1;
	}

	// Convert it to gray
	cvtColor(srcImage, srcImage_gray, CV_BGR2GRAY);

	// Reduce the noise so we avoid false circle detection
	GaussianBlur(srcImage_gray, srcImage_gray, Size(9, 9), 2, 2);

	//"board.png" values
	int dist = 18;
	int threshHigh = 40;
	int threshCenter = 8;
	int minRad = 9;
	int maxRad = 14;

	//"board2.png" values
	//int dist = 20;
	//int threshHigh = 30;
	//int threshCenter = 10;
	//int minRad = 9;
	//int maxRad = 12;

	//display controller
	//namedWindow("Control Display", CV_WINDOW_AUTOSIZE);
	//cvCreateTrackbar("Distance", "Control Display", &dist, 50);
	//cvCreateTrackbar("Upper ThreshHold", "Control Display", &threshHigh, 255);
	//cvCreateTrackbar("ThreshHold Center", "Control Display", &threshCenter, 255);
	//cvCreateTrackbar("Min Radius", "Control Display", &minRad, 255);
	//cvCreateTrackbar("Max Radius", "Control Display", &maxRad, 255);

	//setTrackbarMin("Distance", "Control Display", 1);
	//setTrackbarMin("Upper ThreshHold", "Control Display", 1);
	//setTrackbarMin("ThreshHold Center", "Control Display", 1);

	//internal storing of grid values
	int board[121];

	/////////////////////////////////////////////////////////////////////////////////////
	// Colour:green
	int gLowH = 50;
	int gHighH = 90;
	int gLowS = 90;
	int gHighS = 255;
	int gLowV = 0;
	int gHighV = 255;
	/////////////////////////////////////////////////////////////////////////////////////
	// Colour:orange
	int oLowH = 15;
	int oHighH = 24;
	int oLowS = 90;
	int oHighS = 255;
	int oLowV = 100;
	int oHighV = 255;
	/////////////////////////////////////////////////////////////////////////////////////
	// Colour:yellow
	int yLowH = 24;
	int yHighH = 50;
	int yLowS = 65;
	int yHighS = 255;
	int yLowV = 140;
	int yHighV = 255;
	/////////////////////////////////////////////////////////////////////////////////////
	// Colour:black
	int bLowH = 0;
	int bHighH = 179;
	int bLowS = 0;
	int bHighS = 255;
	int bLowV = 0;
	int bHighV = 80;
	/////////////////////////////////////////////////////////////////////////////////////
	// Colour:blue
	int aLowH = 105;
	int aHighH = 120;
	int aLowS = 65;
	int aHighS = 255;
	int aLowV = 90;
	int aHighV = 255;
	/////////////////////////////////////////////////////////////////////////////////////
	// Colour:white
	int wLowH = 80;
	int wHighH = 110;
	int wLowS = 0;
	int wHighS = 160;
	int wLowV = 100;
	int wHighV = 255;
	/////////////////////////////////////////////////////////////////////////////////////

	//green
	namedWindow("Control Green", CV_WINDOW_AUTOSIZE);
	//Hue (0 - 179)
	cvCreateTrackbar("Low Hue", "Control Green", &gLowH, 179);
	cvCreateTrackbar("High Hue", "Control Green", &gHighH, 179);
	//Saturation (0 - 255)
	cvCreateTrackbar("Low Saturation", "Control Green", &gLowS, 255);
	cvCreateTrackbar("High Saturation", "Control Green", &gHighS, 255);
	//Value (0 - 255)
	cvCreateTrackbar("Low Value", "Control Green", &gLowV, 255);
	cvCreateTrackbar("High Value", "Control Green", &gHighV, 255);

	//orange
	namedWindow("Control Orange", CV_WINDOW_AUTOSIZE);
	//Hue (0 - 179)
	cvCreateTrackbar("Low Hue", "Control Orange", &oLowH, 179);
	cvCreateTrackbar("High Hue", "Control Orange", &oHighH, 179);
	//Saturation (0 - 255)
	cvCreateTrackbar("Low Saturation", "Control Orange", &oLowS, 255);
	cvCreateTrackbar("High Saturation", "Control Orange", &oHighS, 255);
	//Value (0 - 255)
	cvCreateTrackbar("Low Value", "Control Orange", &oLowV, 255);
	cvCreateTrackbar("High Value", "Control Orange", &oHighV, 255);

	//yellow
	namedWindow("Control Yellow", CV_WINDOW_AUTOSIZE);
	//Hue (0 - 179)
	cvCreateTrackbar("Low Hue", "Control Yellow", &yLowH, 179);
	cvCreateTrackbar("High Hue", "Control Yellow", &yHighH, 179);
	//Saturation (0 - 255)
	cvCreateTrackbar("Low Saturation", "Control Yellow", &yLowS, 255);
	cvCreateTrackbar("High Saturation", "Control Yellow", &yHighS, 255);
	//Value (0 - 255)
	cvCreateTrackbar("Low Value", "Control Yellow", &yLowV, 255);
	cvCreateTrackbar("High Value", "Control Yellow", &yHighV, 255);

	//black
	namedWindow("Control Black", CV_WINDOW_AUTOSIZE);
	//Hue (0 - 179)
	cvCreateTrackbar("Low Hue", "Control Black", &bLowH, 179);
	cvCreateTrackbar("High Hue", "Control Black", &bHighH, 179);
	//Saturation (0 - 255)
	cvCreateTrackbar("Low Saturation", "Control Black", &bLowS, 255);
	cvCreateTrackbar("High Saturation", "Control Black", &bHighS, 255);
	//Value (0 - 255)
	cvCreateTrackbar("Low Value", "Control Black", &bLowV, 255);
	cvCreateTrackbar("High Value", "Control Black", &bHighV, 255);

	//blue
	namedWindow("Control Blue", CV_WINDOW_AUTOSIZE);
	//Hue (0 - 179)
	cvCreateTrackbar("Low Hue", "Control Blue", &aLowH, 179);
	cvCreateTrackbar("High Hue", "Control Blue", &aHighH, 179);
	//Saturation (0 - 255)
	cvCreateTrackbar("Low Saturation", "Control Blue", &aLowS, 255);
	cvCreateTrackbar("High Saturation", "Control Blue", &aHighS, 255);
	//Value (0 - 255)
	cvCreateTrackbar("Low Value", "Control Blue", &aLowV, 255);
	cvCreateTrackbar("High Value", "Control Blue", &aHighV, 255);

	//white
	namedWindow("Control White", CV_WINDOW_AUTOSIZE);
	//Hue (0 - 179)
	cvCreateTrackbar("Low Hue", "Control White", &wLowH, 179);
	cvCreateTrackbar("High Hue", "Control White", &wHighH, 179);
	//Saturation (0 - 255)
	cvCreateTrackbar("Low Saturation", "Control White", &wLowS, 255);
	cvCreateTrackbar("High Saturation", "Control White", &wHighS, 255);
	//Value (0 - 255)
	cvCreateTrackbar("Low Value", "Control White", &wLowV, 255);
	cvCreateTrackbar("High Value", "Control White", &wHighV, 255);

	/////////////////////////////////////////////////////////////////////////////////////

	//ofstream outStream("gridTest");
	ifstream inStream("grid");
	Mat mask;

	int loadedCircles[121][3];

	for (int i = 0; i < 121; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			inStream >> loadedCircles[i][j];
		}
	}

	Mat srcImage_circles = srcImage.clone();

	while (true)
	{
		bool bSuccess = vid.read(frame);

		if (!bSuccess)
		{
			cout << "Cannot read a frame from video stream" << endl;
			break;
		}

		//populates markerCorners and markerIds
		aruco::detectMarkers(frame, markerDict, markerCorners, markerIds, parameters, rejectedCandidates, cameraMatrix, distCoefficients);

		if (markerCorners.size() >= 4)
		{
			Point2f src_vertices[4];

			for (int i = 0; i < 4; i++)
			{
				if (markerIds[i] == 0)
				{
					src_vertices[0] = markerCorners[i][1];
				}
			}
			for (int i = 0; i < 4; i++)
			{
				if (markerIds[i] == 1)
				{
					src_vertices[1] = markerCorners[i][2];
				}
			}
			for (int i = 0; i < 4; i++)
			{
				if (markerIds[i] == 2)
				{
					src_vertices[2] = markerCorners[i][3];
				}
			}
			for (int i = 0; i < 4; i++)
			{
				if (markerIds[i] == 3)
				{
					src_vertices[3] = markerCorners[i][0];
				}
			}
			

			Point2f dst_vertices[4];
			dst_vertices[0] = Point2f(300 + 102, 250 - 152);
			dst_vertices[1] = Point2f(300 + 102, 250 + 152);
			dst_vertices[2] = Point2f(300 - 100, 250 + 152);
			dst_vertices[3] = Point2f(300 - 100, 250 - 150);

			Mat prespTrans = getPerspectiveTransform(src_vertices, dst_vertices);

			warpPerspective(frame, warpedDisplay, prespTrans, Size(frame.cols, frame.rows));

			////////////////////////////////////////////////////////////////////////

			Mat imgHSV;

			cvtColor(warpedDisplay, imgHSV, COLOR_BGR2HSV); //Convert the captured frame from BGR to HSV

			////////////////////////////////////////////////////////////////////////
			//colors

			//green
			Mat imgThresholdedGreen;

			inRange(imgHSV, Scalar(gLowH, gLowS, gLowV), Scalar(gHighH, gHighS, gHighV), imgThresholdedGreen);

			//morphological opening (remove small objects from the foreground)
			erode(imgThresholdedGreen, imgThresholdedGreen, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
			dilate(imgThresholdedGreen, imgThresholdedGreen, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));

			//morphological closing (fill small holes in the foreground)
			dilate(imgThresholdedGreen, imgThresholdedGreen, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
			erode(imgThresholdedGreen, imgThresholdedGreen, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));

			//orange
			Mat imgThresholdedOrange;

			inRange(imgHSV, Scalar(oLowH, oLowS, oLowV), Scalar(oHighH, oHighS, oHighV), imgThresholdedOrange);

			//morphological opening (remove small objects from the foreground)
			erode(imgThresholdedOrange, imgThresholdedOrange, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
			dilate(imgThresholdedOrange, imgThresholdedOrange, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));

			//morphological closing (fill small holes in the foreground)
			dilate(imgThresholdedOrange, imgThresholdedOrange, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
			erode(imgThresholdedOrange, imgThresholdedOrange, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));

			//yellow
			Mat imgThresholdedYellow;

			inRange(imgHSV, Scalar(yLowH, yLowS, yLowV), Scalar(yHighH, yHighS, yHighV), imgThresholdedYellow);

			//morphological opening (remove small objects from the foreground)
			erode(imgThresholdedYellow, imgThresholdedYellow, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
			dilate(imgThresholdedYellow, imgThresholdedYellow, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));

			//morphological closing (fill small holes in the foreground)
			dilate(imgThresholdedYellow, imgThresholdedYellow, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
			erode(imgThresholdedYellow, imgThresholdedYellow, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));

			//black
			Mat imgThresholdedBlack;

			inRange(imgHSV, Scalar(bLowH, bLowS, bLowV), Scalar(bHighH, bHighS, bHighV), imgThresholdedBlack);

			//morphological opening (remove small objects from the foreground)
			erode(imgThresholdedBlack, imgThresholdedBlack, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
			dilate(imgThresholdedBlack, imgThresholdedBlack, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));

			//morphological closing (fill small holes in the foreground)
			dilate(imgThresholdedBlack, imgThresholdedBlack, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
			erode(imgThresholdedBlack, imgThresholdedBlack, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));

			//blue
			Mat imgThresholdedBlue;

			inRange(imgHSV, Scalar(aLowH, aLowS, aLowV), Scalar(aHighH, aHighS, aHighV), imgThresholdedBlue);

			//morphological opening (remove small objects from the foreground)
			erode(imgThresholdedBlue, imgThresholdedBlue, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
			dilate(imgThresholdedBlue, imgThresholdedBlue, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));

			//morphological closing (fill small holes in the foreground)
			dilate(imgThresholdedBlue, imgThresholdedBlue, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
			erode(imgThresholdedBlue, imgThresholdedBlue, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));

			//white
			Mat imgThresholdedWhite;

			inRange(imgHSV, Scalar(wLowH, wLowS, wLowV), Scalar(wHighH, wHighS, wHighV), imgThresholdedWhite);

			//morphological opening (remove small objects from the foreground)
			erode(imgThresholdedWhite, imgThresholdedWhite, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
			dilate(imgThresholdedWhite, imgThresholdedWhite, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));

			//morphological closing (fill small holes in the foreground)
			dilate(imgThresholdedWhite, imgThresholdedWhite, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
			erode(imgThresholdedWhite, imgThresholdedWhite, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));


			///////////////////////////////////////////////////////////////////////
			//find coords to create grid

			//vector<Vec3f> oCircles;

			// Apply the Hough Transform to find the circles
			//HoughCircles(imgThresholdedBlue, oCircles, CV_HOUGH_GRADIENT, 1, imgThresholdedBlue.rows / oDist, oThreshHigh, oThreshCenter, oMinRad, oMaxRad);

			//Mat warpedDisplay_circles = warpedDisplay.clone();

			// Draw the circles detected
			//for (size_t i = 0; i < oCircles.size(); i++) {

			//	Point center(cvRound(oCircles[i][0]), cvRound(oCircles[i][1]));
			//	int radius = cvRound(oCircles[i][2]);

			//	circle(warpedDisplay_circles, center, radius, Scalar(197, 97, 33), -1, 8, 0);

			//imshow("Warped Display circles", warpedDisplay_circles);

			//	//'space' key pressed print coords to file
			//	if (waitKey(30) > 5) {
			//		outStream << center;
			//		outStream << radius << endl;
			//	}
			//}

			//////////////////////////////////////////////////////////////////////
			//use grid to detect pieces

			for (int i = 0; i < 121; i++)
			{
				Mat foundPixels;

				mask = Scalar(0, 0, 0) & imgThresholdedBlack.clone();

				Point center(cvRound(loadedCircles[i][0]), cvRound(loadedCircles[i][1]));
				int radius = cvRound(loadedCircles[i][2]);

				//see the whole grid DEBUG
				//circle(warpedDisplay, center, radius, Scalar(255, 255, 255), -1, 8, 0);

				circle(mask, center, radius, Scalar(255, 255, 255), -1, 8, 0);

				board[i] = 0;

				//green
				foundPixels = mask & imgThresholdedGreen;
				if (sum(foundPixels)[0] > 40000)
				{
					board[i] = 1;
					continue;
				}
				//orange
				foundPixels = mask & imgThresholdedOrange;
				if (sum(foundPixels)[0] > 40000)
				{
					double tmp = sum(foundPixels)[0];
					foundPixels = mask & imgThresholdedYellow;
					if (sum(foundPixels)[0] > tmp)
					{
						board[i] = 3;
					}
					else {
						board[i] = 2;
					}
					continue;
				}
				//yellow
				foundPixels = mask & imgThresholdedYellow;
				if (sum(foundPixels)[0] > 40000)
				{
					double tmp = sum(foundPixels)[0];
					foundPixels = mask & imgThresholdedOrange;
					if (sum(foundPixels)[0] > tmp)
					{
						board[i] = 2;
					}
					else {
						board[i] = 3;
					}
					continue;
				}
				//black
				foundPixels = mask & imgThresholdedBlack;
				if (sum(foundPixels)[0] > 40000)
				{
					board[i] = 4;
					continue;
				}
				//blue
				foundPixels = mask & imgThresholdedBlue;
				if (sum(foundPixels)[0] > 40000)
				{
					board[i] = 5;
					continue;
				}
				//white
				foundPixels = mask & imgThresholdedWhite;
				if (sum(foundPixels)[0] > 40000)
				{
					board[i] = 6;
					continue;
				}
			}

			/////////////////////////////////////////////////////////////////////////////////////
			// Display

			vector<Vec3f> circles;

			// Apply the Hough Transform to find the circles in the display image
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

			srcImage_circles = srcImage.clone();

			// Draw the circles detected according to their color in the real life board
			for (size_t i = 0; i < circles.size(); i++)
			{
				Point center(cvRound(circles[i][0]), cvRound(circles[i][1]));
				int radius = cvRound(circles[i][2]);

				switch (board[i]) {

				case 1:	//green
					circle(srcImage_circles, center, radius, Scalar(128, 163, 99), -1, 8, 0);
					break;
				case 2:	//orange
					circle(srcImage_circles, center, radius, Scalar(0, 138, 239), -1, 8, 0);
					break;
				case 3:	//yellow
					circle(srcImage_circles, center, radius, Scalar(62, 205, 232), -1, 8, 0);
					break;
				case 4:	//black
					circle(srcImage_circles, center, radius, Scalar(19, 18, 22), -1, 8, 0);
					break;
				case 5: //blue
					circle(srcImage_circles, center, radius, Scalar(197, 97, 33), -1, 8, 0);
					break;
				case 6:	//white
					circle(srcImage_circles, center, radius, Scalar(242, 232, 222), -1, 8, 0);
					break;
				}

			}
			//DEBUG threshold
			//imshow("Warped Display Thresholded", imgThresholdedBlue);
		}
		else
		{
			Mat tmpWarning;

			resize(warning, warning, Size(frame.cols, frame.rows));

			cvtColor(warning, tmpWarning, COLOR_BGR2RGB);

			addWeighted(tmpWarning, 1, frame, 0.2, 0.0, warpedDisplay);

		}

		
		//getting the pose to draw to the camera ref
		aruco::estimatePoseSingleMarkers(markerCorners, arucoSquareDimension, cameraMatrix, distCoefficients, rotationVectors, translationVectors);

		//find one of the 50 markers and draw the axis
		for (int i = 0; i < markerIds.size(); i++)
		{
			aruco::drawAxis(frame, cameraMatrix, distCoefficients, rotationVectors[i], translationVectors[i], 0.1f);
		}


		//////////////////////////////////////////////////////////////////////

		imshow("Webcam", frame);
		imshow("Warped Display", warpedDisplay);
		imshow("Info display", srcImage_circles);


		//'esc' key pressed for 30ms
		if (waitKey(30) == 27) {
			cout << "esc key is pressed by user" << endl;
			//outStream.close();
			inStream.close();
			break;
		}
	}
	return 1;
}

//loads the camera calibration values from a file
bool loadCamCalib(string name, Mat& cameraMatrix, Mat& distCoefficients)
{
	ifstream inStream(name);
	if (inStream)
	{
		uint16_t rows;
		uint16_t cols;

		inStream >> rows;
		inStream >> cols;

		cameraMatrix = Mat(Size(cols, rows), CV_64F);

		for (int r = 0; r < rows; r++)
		{
			for (int c = 0; c < cols; c++)
			{
				double read = 0.0f;
				inStream >> read;
				cameraMatrix.at<double>(r, c) = read;
				cout << cameraMatrix.at<double>(r, c) << "\n";
			}
		}
		//Distance coeficients
		inStream >> rows;
		inStream >> cols;

		distCoefficients = Mat::zeros(rows, cols, CV_64F);

		for (int r = 0; r < rows; r++)
		{
			for (int c = 0; c < cols; c++)
			{
				double read = 0.0f;
				inStream >> read;
				distCoefficients.at<double>(r, c) = read;
				cout << distCoefficients.at<double>(r, c) << "\n";
			}
		}
		inStream.close();
		return true;
	}
	return false;
}

int main(int argv, char** argc)
{
	Mat cameraMatrix = Mat::eye(3, 3, CV_64F);

	Mat distCoefficients;

	//calibWebcam(cameraMatrix, distCoefficients);
	
	loadCamCalib("CamCalibValues", cameraMatrix, distCoefficients);
	startWebcamMonitoring(cameraMatrix, distCoefficients, arucoSquareDimension);

	return 0;
}