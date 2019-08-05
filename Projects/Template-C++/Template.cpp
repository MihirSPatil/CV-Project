#include <fstream>
#include <iostream>
#include <opencv2/opencv.hpp>

struct DetectedDie
{
	// Some pixel position within the die where it is visible (i.e. not occluded).
	cv::Point somePositionWithin;

	// The number of dots shown by the die's top face (1 to 6).
	uint value;
};

struct DetectionResult
{
	// Zero-based number of the video frame that should be shown when displaying the detection results (one of the frames that you used for dice detection).
	uint referenceFrameNo;

	// One item for each die that you detected.
	std::vector<DetectedDie> detectedDice;
};

// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// ! You should not change anything ABOVE this point. !
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

// TODO: Implement this function!
DetectionResult detectDice(cv::VideoCapture& videoCapture)
{
	// TODO: Detect the dice in the video provided! Return the result like shown here (also see struct definitions above):
	DetectionResult detectionResult;
	detectionResult.referenceFrameNo = 42;
	detectionResult.detectedDice.push_back({ cv::Point(100, 200), 3 });
	detectionResult.detectedDice.push_back({ cv::Point(400, 500), 6 });
	return detectionResult;
}

// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// ! You should not change anything BELOW this point. !
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

int main(int numArgs, const char** pp_args)
{
	if (numArgs != 3)
	{
		std::cerr << "Invalid command line arguments: Specify the video filename and the detection result filename!" << std::endl;
		return 1;
	}

	const std::string videoFilename = pp_args[1];
	const std::string detectionResultFilename = pp_args[2];

	cv::VideoCapture videoCapture(videoFilename);
	if (!videoCapture.isOpened())
	{
		std::cerr << "Failed to open video file \"" << videoFilename << "\"!" << std::endl;
		return 1;
	}

	DetectionResult detectionResult = detectDice(videoCapture);

	std::ofstream fileStream(detectionResultFilename);
	fileStream << detectionResult.referenceFrameNo << std::endl;
	fileStream << detectionResult.detectedDice.size() << std::endl;
	for (const DetectedDie& detectedDie : detectionResult.detectedDice)
		fileStream << detectedDie.somePositionWithin.x << ' ' << detectedDie.somePositionWithin.y << ' ' << detectedDie.value << std::endl;
	if (!fileStream)
	{
		std::cerr << "Failed to open/create/write detection result file \"" << detectionResultFilename << "\"!" << std::endl;
		return 1;
	}

	return 0;
}