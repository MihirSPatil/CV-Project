#include <cstdlib>
#include <ctime>
#include <experimental/filesystem>
#include <fstream>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <json.hpp>
#if _WIN32
#include <Windows.h>
#elif __unix__
#else
#error Unknown platform!
#endif

namespace fs = std::experimental::filesystem;

struct DetectedDie
{
	cv::Point somePositionWithin;
	uint value;
};

struct DetectionResult
{
	uint referenceFrameNo;
	std::vector<DetectedDie> detectedDice;
};

struct GroundtruthDie
{
	std::vector<cv::Point> contourPoints;
	uint value;
};

struct Groundtruth
{
	uint referenceFrameNo;
	std::vector<GroundtruthDie> groundtruthDice;
};

struct Competitor
{
	std::string name;
	std::string executablePath;
	bool currentVideoDone = false;
	int currentVideoScore = 0;
	int totalScore = 0;
	uint numVideosTested = 0;
	uint currentRank = 0;
};

template <typename T> T fromString(const std::string& string)
{
	std::istringstream stream(string);
	T result;
	stream >> result;
	if (!stream)
		throw std::runtime_error("Could not convert string \"" + string + "\" to type \"" + typeid(T).name() + "\"!");
	return result;
}

void putTextShadow(cv::InputOutputArray img, const cv::String& text, cv::Point org, int fontFace, double fontScale, cv::Scalar color, cv::Scalar shadowColor = cv::Scalar(0, 0, 0), int thickness = 1, int shadowThickness = 3, int lineType = cv::LINE_8, bool bottomLeftOrigin = false)
{
	cv::putText(img, text, org, fontFace, fontScale, shadowColor, shadowThickness, lineType, bottomLeftOrigin);
	cv::putText(img, text, org, fontFace, fontScale, color, thickness, lineType, bottomLeftOrigin);
}

DetectionResult loadDetectionResult(const std::string& filename)
{
	DetectionResult detectionResult;
	std::ifstream file(filename);
	file >> detectionResult.referenceFrameNo;
	size_t numDice;
	file >> numDice;
	for (size_t i = 0; i < numDice; ++i)
	{
		DetectedDie detectedDie;
		file >> detectedDie.somePositionWithin.x >> detectedDie.somePositionWithin.y >> detectedDie.value;
		detectionResult.detectedDice.push_back(detectedDie);
	}

	if (!file)
		throw std::runtime_error("Failed to open/read/parse detection result file \"" + filename + "\"!");

	return detectionResult;
}

Groundtruth loadGroundtruth(const std::string& filename)
{
	std::ifstream file(filename);
	nlohmann::json json;
	file >> json;

	if (!file)
		throw std::runtime_error("Failed to open/read/parse groundtruth file \"" + filename + "\"!");

	try
	{
		Groundtruth groundtruth;
		std::string flag = json["flags"].begin().key();
		groundtruth.referenceFrameNo = fromString<uint>(flag.substr(flag.find('=') + 1));
		
		for (const auto& shape : json["shapes"])
		{
			GroundtruthDie groundtruthDie;
			groundtruthDie.value = fromString<uint>(shape["label"].get<std::string>());
			if (groundtruthDie.value < 1 || groundtruthDie.value > 6)
				throw std::runtime_error("Invalid die value: " + std::to_string(groundtruthDie.value));
			for (const auto& point : shape["points"])
				groundtruthDie.contourPoints.emplace_back(point[0], point[1]);
			groundtruth.groundtruthDice.push_back(groundtruthDie);
		}

		return groundtruth;
	}
	catch (const std::exception& exception)
	{
		throw std::runtime_error("Failed to open/read/parse groundtruth file \"" + filename + "\"! Inner exception: " + exception.what());
	}
}

bool callCompetitor(const Competitor& competitor, const std::string& videoBasename, const std::string& resultsDirectory, uint timeoutMs, DetectionResult& outDetectionResult, uint& outRunningTime)
{
	std::string detectionResultFilename = resultsDirectory + '/' + fs::path(videoBasename).filename().string() + " - " + competitor.name + ".txt";
	
	if (fs::is_regular_file(detectionResultFilename))
		fs::remove(detectionResultFilename);

#if _WIN32
	SHELLEXECUTEINFOA shellExecuteInfo = { sizeof(shellExecuteInfo) };
	shellExecuteInfo.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI | SEE_MASK_NO_CONSOLE;
	shellExecuteInfo.lpVerb = "open";
	shellExecuteInfo.lpFile = competitor.executablePath.c_str();
	std::string parameters = "\"" + videoBasename + ".avi\" \"" + detectionResultFilename + '"';
	shellExecuteInfo.lpParameters = parameters.c_str();
#elif __unix__
	std::string commandLine = "timeout --signal=KILL " + std::to_string(timeoutMs / 1000.0) + "s \"" + competitor.executablePath + "\" \"" + videoBasename + ".avi\" \"" + detectionResultFilename + '"';
#endif

	int64 t0 = cv::getTickCount();

#ifdef _WIN32
	if (ShellExecuteExA(&shellExecuteInfo))
		if (WaitForSingleObject(shellExecuteInfo.hProcess, timeoutMs) == WAIT_TIMEOUT)
			TerminateProcess(shellExecuteInfo.hProcess, 1);
#elif __unix__
	int unusedResult = std::system(commandLine.c_str());
#endif

	outRunningTime = static_cast<uint>(1000 * (cv::getTickCount() - t0) / cv::getTickFrequency());

	if (!fs::is_regular_file(detectionResultFilename))
		return false;

	try
	{
		outDetectionResult = loadDetectionResult(detectionResultFilename);
	}
	catch (const std::exception&)
	{
		return false;
	}

	return true;
}

int computeScore(const DetectionResult& detectionResult, const Groundtruth& groundtruth, std::vector<bool>& outCompletelyWrong, std::vector<bool>& outHitByAnyDetection, std::vector<bool>& outClassifiedCorrectly)
{
	size_t numDetectedDice = detectionResult.detectedDice.size();
	size_t numGroundtruthDice = groundtruth.groundtruthDice.size();
	outCompletelyWrong.assign(detectionResult.detectedDice.size(), true);
	outHitByAnyDetection.assign(numGroundtruthDice, false);
	outClassifiedCorrectly.assign(numGroundtruthDice, false);
	
	for (size_t i = 0; i < numDetectedDice; ++i)
	{
		const DetectedDie& detectedDie = detectionResult.detectedDice[i];
		for (size_t j = 0; j < numGroundtruthDice; ++j)
		{
			const GroundtruthDie& groundtruthDie = groundtruth.groundtruthDice[j];
			if (cv::pointPolygonTest(groundtruthDie.contourPoints, detectedDie.somePositionWithin, false) >= 0)
			{
				outCompletelyWrong[i] = false;
				outHitByAnyDetection[j] = true;
				if (detectedDie.value == groundtruthDie.value)
					outClassifiedCorrectly[j] = true;
			}
		}
	}

	return 2 * static_cast<int>(std::count(outHitByAnyDetection.begin(), outHitByAnyDetection.end(), true)) + static_cast<int>(std::count(outClassifiedCorrectly.begin(), outClassifiedCorrectly.end(), true)) - static_cast<int>(detectionResult.detectedDice.size());
}

void updateRankingWindow(const std::string& windowName, const cv::Size& frameSize, std::vector<Competitor>& competitors, int currentVideoNo, size_t numVideos, int currentCompetitorNo, uint maximumScore, uint maximumTotalScore)
{
	cv::Mat3b frame(frameSize, cv::Vec3b(0, 0, 0));
	std::string label = (currentVideoNo == -1) ? ("Final scores (max. " + std::to_string(maximumTotalScore) + " points in total)") : ("Scores - evaluation video " + std::to_string(currentVideoNo + 1) + " out of " + std::to_string(numVideos) + " (max. " + std::to_string(maximumScore) + " points)");
	cv::Point labelPosition(20, 75);
	frame.rowRange(0, 120).setTo(64);
	putTextShadow(frame, label, labelPosition, cv::FONT_HERSHEY_SIMPLEX, 1.75, cv::Scalar(255, 255, 255), cv::Scalar(0, 0, 0), 3, 10, cv::LINE_AA);
	labelPosition.y += 110;
	
	std::vector<Competitor*> sortedCompetitors;
	for (Competitor& competitor : competitors)
		sortedCompetitors.push_back(&competitor);
	std::sort(sortedCompetitors.begin(), sortedCompetitors.end(), [&](Competitor* p_x, Competitor* p_y) { return p_x->totalScore > p_y->totalScore; });
	uint rank = 1;
	uint numCompetitorsWithThisScore = 1;
	for (size_t i = 0; i < sortedCompetitors.size(); ++i)
	{
		Competitor& competitor = *sortedCompetitors[i];
		competitor.currentRank = rank;
		if (i != sortedCompetitors.size() - 1 && competitor.totalScore == sortedCompetitors[i + 1]->totalScore)
			++numCompetitorsWithThisScore;
		else
		{
			rank += numCompetitorsWithThisScore;
			numCompetitorsWithThisScore = 1;
		}
	}
	
	for (size_t i = 0; i < competitors.size(); ++i)
	{
		const Competitor& competitor = competitors[i];
		label = '#' + std::to_string(competitor.currentRank) + " - \"" + competitor.name + "\": " + std::to_string(competitor.totalScore);
		cv::Scalar labelColor;
		if (currentVideoNo == -1)
			labelColor = cv::Scalar(255, 255, 255);
		else if (competitor.numVideosTested == currentVideoNo + 1)
			labelColor = cv::Scalar(255, 255, 255);
		else
			labelColor = i == currentCompetitorNo ? cv::Scalar(255, 128, 0) : cv::Scalar(128, 128, 128);
		if (competitor.currentVideoDone)
			label += std::string(" (") + (competitor.currentVideoScore > 0 ? "+" : "") + std::to_string(competitor.currentVideoScore) + ')';

		cv::putText(frame, label, labelPosition, cv::FONT_HERSHEY_SIMPLEX, 1.75, labelColor, 3, cv::LINE_AA);
		
		if (competitor.totalScore > 0)
		{
			int full = static_cast<int>((frameSize.width - 2 * labelPosition.x) * competitor.totalScore / maximumTotalScore);
			int part = static_cast<int>((frameSize.width - 2 * labelPosition.x) * competitor.currentVideoScore / maximumTotalScore);
			if (currentVideoNo == -1)
				part = full;
			if (full - part > 0)
				cv::rectangle(frame, cv::Rect(labelPosition.x, labelPosition.y + 20, full - part, 10), 0.5 * labelColor, -1);
			if (part > 0)
				cv::rectangle(frame, cv::Rect(labelPosition.x + full - part, labelPosition.y + 20, part, 10), labelColor, -1);
		}
		
		labelPosition.y += 70;
	}

	cv::imshow(windowName, frame);
}

int main(int numArgs, const char** pp_args)
{
	if (numArgs < 5 || numArgs % 2 != 1)
	{
		std::cerr << "Invalid command line arguments: Specify the competitors (name and executable path for each one) followed by the directory containing the evaluation data and the directory that will contain the output!" << std::endl;
		return 1;
	}

	std::string videoWindowName = "Dice Detection Evaluation - Video";
	cv::namedWindow(videoWindowName, cv::WINDOW_NORMAL);
	cv::resizeWindow(videoWindowName, 1936, 1216);
	
	std::string rankingWindowName = "Dice Detection Evaluation - Ranking";
	cv::namedWindow(rankingWindowName, cv::WINDOW_NORMAL);
	cv::Size rankingFrameSize(1920, 1080);
	cv::resizeWindow(rankingWindowName, rankingFrameSize.width, rankingFrameSize.height);
	
	std::vector<Competitor> competitors;
	for (int i = 1; i < numArgs - 2; i += 2)
	{
		Competitor competitor;
		competitor.name = pp_args[i];
		competitor.executablePath = pp_args[i + 1];
		if (!fs::is_regular_file(competitor.executablePath))
		{
			std::cerr << "The provided executable path \"" << competitor.executablePath << "\" for competitor \"" << competitor.name << "\" is not a regular file!" << std::endl;
			return 1;
		}

		competitors.push_back(competitor);
		std::cout << "Added competitor \"" << competitor.name << "\" with executable path \"" << competitor.executablePath << "\"." << std::endl;
	}

	std::string evaluationDataDirectory = pp_args[numArgs - 2];
	if (!fs::is_directory(evaluationDataDirectory))
	{
		std::cerr << "The provided evaluation data directory path \"" << evaluationDataDirectory << "\" is not a directory!" << std::endl;
		return 1;
	}

	std::string resultsDirectory = pp_args[numArgs - 1];
	if (!fs::is_directory(resultsDirectory))
	{
		std::cerr << "The provided results directory path \"" << resultsDirectory << "\" is not a directory!" << std::endl;
		return 1;
	}

	std::cout << "We have " << competitors.size() << " competitors." << std::endl;
	std::cout << std::endl;

	std::vector<std::pair<std::string, Groundtruth>> evaluationData;
	for (const auto& directoryEntry : fs::recursive_directory_iterator(evaluationDataDirectory))
	{
		const auto& path = directoryEntry.path();
		if (fs::is_regular_file(directoryEntry) && path.extension() == ".avi")
		{
			auto pngPath = path;
			pngPath.replace_extension(".png");
			auto jsonPath = path;
			jsonPath.replace_extension(".json");
			if (!fs::is_regular_file(pngPath) || !fs::is_regular_file(jsonPath))
			{
				std::cerr << "Missing reference frame and/or JSON file for the video file \"" << path.string() << "\"!" << std::endl;
				return 1;
			}

			auto pathWithoutExtension = path;
			pathWithoutExtension.replace_extension();
			evaluationData.emplace_back(pathWithoutExtension.string(), loadGroundtruth(jsonPath.string()));
			std::cout << "Added evaluation video \"" << path.string() << "\"." << std::endl;
		}
	}

	if (evaluationData.empty())
	{
		std::cerr << "No evaluation data found in directory \"" << evaluationDataDirectory << "\". Put some data there!" << std::endl;
		return 1;
	}
	
	std::cout << "We have " << evaluationData.size() << " evaluation videos." << std::endl;

	time_t now = time(0);
	tm* p_timeStruct = localtime(&now);
	char buffer[256];
	strftime(buffer, sizeof(buffer), "%Y-%m-%d@%H-%M-%S", p_timeStruct);
	resultsDirectory += '/';
	resultsDirectory += buffer;
	fs::create_directory(resultsDirectory);
	std::ofstream csvFile(resultsDirectory + "/Results.csv");
	for (const Competitor& competitor : competitors)
		csvFile << ',' << competitor.name;
	csvFile << std::endl;

	uint maximumTotalScore = 0;
	for (const auto& evaluationItem : evaluationData)
		maximumTotalScore += 2 * static_cast<uint>(evaluationItem.second.groundtruthDice.size());

	for (size_t i = 0; i < evaluationData.size(); ++i)
	{
		const auto& evaluationItem = evaluationData[i];
		std::string videoFilename = evaluationItem.first + ".avi";
		cv::VideoCapture videoCapture(videoFilename);
		if (!videoCapture.isOpened())
		{
			std::cerr << "Failed to open video file \"" << videoFilename << "\"!" << std::endl;
			return 1;
		}

		const Groundtruth& groundtruth = evaluationItem.second;
		uint maximumScore = 2 * static_cast<uint>(groundtruth.groundtruthDice.size());

		csvFile << fs::path(videoFilename).filename().replace_extension().string();
		std::cout << std::endl;
		std::cout << "Processing evaluation video \"" << videoFilename << "\" (" << evaluationItem.second.groundtruthDice.size() << " dice, max. " << maximumScore << " points) ..." << std::endl;

		updateRankingWindow(rankingWindowName, rankingFrameSize, competitors, static_cast<int>(i), evaluationData.size(), -1, maximumScore, maximumTotalScore);
		cv::waitKey(1);

		cv::Mat3b frame;
		cv::Size videoSize;
		while (true)
		{
			uint frameNo = static_cast<uint>(videoCapture.get(cv::CAP_PROP_POS_FRAMES));
			videoCapture >> frame;
			if (frame.empty())
				break;
			videoSize = frame.size();
			if (frameNo == groundtruth.referenceFrameNo)
			{
				for (const auto& groundtruthDie : groundtruth.groundtruthDice)
					cv::fillPoly(frame, std::vector<std::vector<cv::Point>>{ groundtruthDie.contourPoints }, cv::Scalar(0, 0, 255), cv::LINE_AA);
				frame *= 2;
			}
			std::string label = "Evaluation video " + std::to_string(i + 1) + " out of " + std::to_string(evaluationData.size()) + " (" + std::to_string(groundtruth.groundtruthDice.size()) + " dice, max. " + std::to_string(maximumScore) + " points)";
			cv::Point labelPosition(20, 75);
			frame.rowRange(0, 120) *= 0.25;
			putTextShadow(frame, label, labelPosition, cv::FONT_HERSHEY_SIMPLEX, 1.75, cv::Scalar(255, 255, 255), cv::Scalar(0, 0, 0), 3, 10, cv::LINE_AA);
			cv::imshow(videoWindowName, frame);
			int keyPressed = cv::waitKey(frameNo == groundtruth.referenceFrameNo ? 250 : 1);
			if (keyPressed != -1 && keyPressed != 255)
				break;
		}

		frame = cv::Mat3b::zeros(videoSize);
		cv::imshow(videoWindowName, frame);
		cv::waitKey(1);

		videoCapture.set(cv::CAP_PROP_POS_FRAMES, groundtruth.referenceFrameNo);
		cv::Mat3b groundtruthReferenceFrame;
		videoCapture >> groundtruthReferenceFrame;

		for (size_t j = 0; j < competitors.size(); ++j)
		{
			Competitor& competitor = competitors[j];
			std::cout << "- Testing competitor \"" << competitor.name << "\" ...";

			frame.setTo(0);
			cv::imshow(videoWindowName, frame);
			cv::waitKey(250);

			groundtruthReferenceFrame.copyTo(frame);
			std::string label = "Testing \"" + competitor.name + "\" (" + std::to_string(j + 1) + " out of " + std::to_string(competitors.size()) + ") ...";
			cv::Point labelPosition(20, 75);
			frame.rowRange(0, 120) *= 0.25;
			putTextShadow(frame, label, labelPosition, cv::FONT_HERSHEY_SIMPLEX, 1.75, cv::Scalar(255, 255, 255), cv::Scalar(0, 0, 0), 3, 10, cv::LINE_AA);
			cv::imshow(videoWindowName, frame);
			updateRankingWindow(rankingWindowName, rankingFrameSize, competitors, static_cast<int>(i), evaluationData.size(), static_cast<int>(j), maximumScore, maximumTotalScore);
			cv::waitKey(250);

			DetectionResult detectionResult;
			uint runningTime;
			bool gotResult = callCompetitor(competitor, evaluationItem.first, resultsDirectory, 15000, detectionResult, runningTime);
			if (gotResult)
			{
				std::vector<bool> completelyWrong;
				std::vector<bool> hitByAnyDetection;
				std::vector<bool> classifiedCorrectly;
				competitor.currentVideoScore = computeScore(detectionResult, groundtruth, completelyWrong, hitByAnyDetection, classifiedCorrectly);

				std::cout << " ref. frame #" << detectionResult.referenceFrameNo << ", " << detectionResult.detectedDice.size() << " dice detected, " << competitor.currentVideoScore << " points" << std::endl;

				cv::Mat3b detectionReferenceFrame;
				if (detectionResult.referenceFrameNo < static_cast<uint>(videoCapture.get(cv::CAP_PROP_FRAME_COUNT)))
				{
					videoCapture.set(cv::CAP_PROP_POS_FRAMES, detectionResult.referenceFrameNo);
					videoCapture >> detectionReferenceFrame;
				}
				else
					detectionReferenceFrame = cv::Mat3b(videoSize, cv::Vec3b(0, 0, 255));
				frame = 0.5 * (detectionReferenceFrame + groundtruthReferenceFrame);

				label = '"' + competitor.name + "\" finished in " + std::to_string(runningTime) + " ms: " + std::to_string(competitor.currentVideoScore) + " points out of " + std::to_string(maximumScore);
				frame.rowRange(0, 120) *= 0.25;
				putTextShadow(frame, label, labelPosition, cv::FONT_HERSHEY_SIMPLEX, 1.75, cv::Scalar(255, 255, 255), cv::Scalar(0, 0, 0), 3, 10, cv::LINE_AA);

				for (size_t k = 0; k < groundtruth.groundtruthDice.size(); ++k)
				{
					const GroundtruthDie& groundtruthDie = groundtruth.groundtruthDice[k];
					cv::Scalar labelColor = cv::Scalar(0, 0, 255);
					if (classifiedCorrectly[k])
						labelColor = cv::Scalar(0, 255, 0);
					else if (hitByAnyDetection[k])
						labelColor = cv::Scalar(0, 255, 255);
					cv::polylines(frame, groundtruthDie.contourPoints, true, cv::Scalar(0, 0, 0), 5, cv::LINE_AA);
					cv::polylines(frame, groundtruthDie.contourPoints, true, labelColor, 2, cv::LINE_AA);
					label = std::to_string(groundtruthDie.value);
					cv::Size labelSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 1.75, 10, nullptr);
					cv::Moments contourMoments = cv::moments(groundtruthDie.contourPoints);
					labelPosition = cv::Point(static_cast<int>(contourMoments.m10 / contourMoments.m00), static_cast<int>(contourMoments.m01 / contourMoments.m00)) + cv::Point(-labelSize.width / 2, labelSize.height / 2);
					cv::putText(frame, label, labelPosition, cv::FONT_HERSHEY_SIMPLEX, 1.75, cv::Scalar(0, 0, 0), 10, cv::LINE_AA);
					cv::putText(frame, label, labelPosition, cv::FONT_HERSHEY_SIMPLEX, 1.75, labelColor, 3, cv::LINE_AA);
				}

				for (size_t k = 0; k < detectionResult.detectedDice.size(); ++k)
				{
					const DetectedDie& detectedDie = detectionResult.detectedDice[k];
					cv::Scalar labelColor = completelyWrong[k] ? cv::Scalar(0, 0, 255) : cv::Scalar(255, 255, 255);
					cv::drawMarker(frame, detectedDie.somePositionWithin, cv::Scalar(0, 0, 0), cv::MARKER_CROSS, 40, 5, cv::LINE_AA);
					cv::drawMarker(frame, detectedDie.somePositionWithin, labelColor, cv::MARKER_CROSS, 40, 2, cv::LINE_AA);
					label = std::to_string(detectedDie.value);
					labelPosition = detectedDie.somePositionWithin + cv::Point(10, -10);
					putTextShadow(frame, label, labelPosition, cv::FONT_HERSHEY_SIMPLEX, 1, labelColor, cv::Scalar(0, 0, 0), 2, 5, cv::LINE_AA);
				}
			}
			else
			{
				competitor.currentVideoScore = 0;

				std::cout << " No result! 0 points" << std::endl;

				frame = 0.5 * (groundtruthReferenceFrame + cv::Mat3b(videoSize, cv::Vec3b(0, 0, 255)));
				label = '"' + competitor.name + "\" gave no result: 0 points out of " + std::to_string(maximumScore);
				frame.rowRange(0, 120) *= 0.25;
				putTextShadow(frame, label, labelPosition, cv::FONT_HERSHEY_SIMPLEX, 1.75, cv::Scalar(255, 255, 255), cv::Scalar(0, 0, 0), 3, 10, cv::LINE_AA);
			}

			competitor.currentVideoDone = true;

			csvFile << ',' << competitor.currentVideoScore;
			competitor.totalScore += competitor.currentVideoScore;

			std::string detectionResultFilename = resultsDirectory + '/' + fs::path(evaluationItem.first).filename().string() + " - " + competitor.name + ".png";
			cv::imwrite(detectionResultFilename, frame);

			cv::imshow(videoWindowName, frame);
			updateRankingWindow(rankingWindowName, rankingFrameSize, competitors, static_cast<int>(i), evaluationData.size(), static_cast<int>(j), maximumScore, maximumTotalScore);
			cv::waitKey(gotResult ? 3000 : 1000);

			++competitor.numVideosTested;
		}

		csvFile << std::endl;

		if (i == evaluationData.size() - 1)
		{
			csvFile << "Total";
			for (const Competitor& competitor : competitors)
				csvFile << ';' << competitor.totalScore;

			frame = cv::Vec3b::all(255);
			cv::imshow(videoWindowName, frame);
		}

		for (Competitor& competitor : competitors)
		{
			competitor.currentVideoDone = false;
			competitor.currentVideoScore = 0;
		}
	}

	updateRankingWindow(rankingWindowName, rankingFrameSize, competitors, -1, 0, -1, 0, maximumTotalScore);
	cv::waitKey();

	std::cout << std::endl;
	std::cout << "Final scores (max. " << maximumTotalScore << " points):" << std::endl;
	for (const auto& competitor : competitors)
		std::cout << "- " << competitor.name << ": " << competitor.totalScore << " points" << std::endl;

	return 0;
}