#include <ctime>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <mvIMPACT_CPP/mvIMPACT_acquire_GenICam.h>

// Source: https://stackoverflow.com/questions/16605967/set-precision-of-stdto-string-when-converting-floating-point-values
template <typename T> std::string toString(T value, int precision = 6)
{
	std::ostringstream out;
	out << std::setprecision(precision) << value;
	return out.str();
}

int main()
{
	const std::string WINDOW_NAME = "VideoRecorder";
	const int KEY_PAGE_UP = 2162688;
	const int KEY_PAGE_DOWN = 2228224;

	mvIMPACT::acquire::DeviceManager deviceManager;
	if (!deviceManager.deviceCount())
	{
		std::cerr << "No cameras found!" << std::endl;
		return 1;
	}

	mvIMPACT::acquire::Device* p_device = deviceManager.getDevice(0);

	std::unique_ptr<mvIMPACT::acquire::FunctionInterface> p_functionInterface = std::make_unique<mvIMPACT::acquire::FunctionInterface>(p_device);
	if (p_functionInterface->loadSetting("Data/mvBlueFOX.xml", mvIMPACT::acquire::sfFile) != mvIMPACT::acquire::DMR_NO_ERROR)
	{
		std::cerr << "Could not load camera configuration!" << std::endl;
		return 1;
	}

	std::unique_ptr<mvIMPACT::acquire::SystemSettings> p_systemSettings = std::make_unique<mvIMPACT::acquire::SystemSettings>(p_device);
	std::unique_ptr<mvIMPACT::acquire::GenICam::AcquisitionControl> p_acquisitionControl = std::make_unique<mvIMPACT::acquire::GenICam::AcquisitionControl>(p_device);
	std::unique_ptr<mvIMPACT::acquire::GenICam::AnalogControl> p_analogControl = std::make_unique<mvIMPACT::acquire::GenICam::AnalogControl>(p_device);
	
	// Fill the request queue.
	for (int i = 0; i < p_systemSettings->requestCount.read(); ++i)
		p_functionInterface->imageRequestSingle();
	
	cv::Mat3b frame;
	cv::Mat3b gui;
	cv::Mat3b frameMarkedForLabeling;
	cv::VideoWriter videoWriter;
	std::string videoFilename;
	int numFramesRecorded = 0;
	int frameNumberMarkedForLabeling = -1;

	while (true)
	{
		int requestId = p_functionInterface->imageRequestWaitFor(-1);
		if (p_functionInterface->isRequestNrValid(requestId))
		{
			mvIMPACT::acquire::Request* p_request = p_functionInterface->getRequest(requestId);
			if (p_request->isOK())
			{
				if (frame.empty())
					frame.create(p_request->imageHeight.read(), p_request->imageWidth.read());
				memcpy(frame.ptr(0), p_request->imageData.read(), p_request->imageSize.read());
			}
			else p_request = nullptr;

			p_functionInterface->imageRequestUnlock(requestId);

			// Fill the request queue.
			while (p_functionInterface->imageRequestSingle() != mvIMPACT::acquire::DEV_NO_FREE_REQUEST_AVAILABLE);
		}

		if (frame.empty())
			continue;
		
		std::vector<std::pair<std::string, cv::Scalar>> textLines;

		if (videoWriter.isOpened())
		{
			videoWriter.write(frame);
			++numFramesRecorded;

			textLines.push_back({ "Recording ...", cv::Scalar(0, 0, 255) });
			textLines.push_back({ "Press [R] to stop and save", cv::Scalar(255, 255, 255) });
			textLines.push_back({ "Press [C] to cancel", cv::Scalar(255, 255, 255) });
			textLines.push_back({ "Press [S] to mark the current frame for labeling" + (frameNumberMarkedForLabeling == -1 ? "" : " (marked frame #" + std::to_string(frameNumberMarkedForLabeling) + ')'), cv::Scalar(255, 255, 255) });
			textLines.push_back({ "Filename: " + videoFilename, cv::Scalar(255, 255, 255) });
			textLines.push_back({ "Number of frames recorded: " + std::to_string(numFramesRecorded), cv::Scalar(255, 255, 255) });
		}
		else
		{
			textLines.push_back({ "Press [R] to start recording a new video", cv::Scalar(255, 255, 255) });
			textLines.push_back({ "Press [Page up]/[Page down] to adjust exposure time - currently " + std::to_string(static_cast<int>(p_acquisitionControl->exposureTime.read())) + " us", cv::Scalar(255, 255, 255) });
			textLines.push_back({ "Press [+]/[-] to adjust sensor gain - currently " + toString(p_analogControl->gain.read(), 4) + " dB", cv::Scalar(255, 255, 255) });
		}

		frame.copyTo(gui);

		// Darken the background (will not be recorded) to make the text visible in any case.
		gui.rowRange(0, 25 * static_cast<int>(textLines.size() + 1)) *= 0.25;
		int y = 30;
		for (const auto& textLine : textLines)
		{
			cv::putText(gui, textLine.first, cv::Point(10, y), cv::FONT_HERSHEY_DUPLEX, 0.6, textLine.second, 1, cv::LINE_AA);
			y += 25;
		}

		cv::imshow(WINDOW_NAME, gui);

		int key = cv::waitKeyEx(1);
		int keyWithoutModifiers = key & 0xFFFF;
		if (keyWithoutModifiers == 27)
			break;
		else if (keyWithoutModifiers == 'r')
		{
			if (videoWriter.isOpened())
			{
				videoWriter.release();
				if (frameNumberMarkedForLabeling != -1)
				{
					std::string screenshotFilename = videoFilename.replace(videoFilename.length() - 3, 3, "png");
					cv::imwrite(screenshotFilename, frameMarkedForLabeling);
					std::string command = "START /B Software/labelme-3.3.6/labelme.exe --nodata --flags frameNo=" + std::to_string(frameNumberMarkedForLabeling) + " --labels 1,2,3,4,5,6 --epsilon 3 \"" + screenshotFilename + '"';
					system(command.c_str());
				}
			}
			else
			{
				time_t now = time(0);
				tm timeStruct;
				localtime_s(&timeStruct, &now);
				char buffer[256];
				strftime(buffer, sizeof(buffer), "Data/Videos/Recorded/%Y-%m-%d@%H-%M-%S.avi", &timeStruct);
				videoFilename = buffer;
				videoWriter.open(videoFilename, cv::VideoWriter::fourcc('H', '2', '6', '4'), 25, frame.size());
				videoWriter.set(cv::VIDEOWRITER_PROP_QUALITY, 100);
				numFramesRecorded = 0;
				frameNumberMarkedForLabeling = -1;
			}
		}
		else if (keyWithoutModifiers == 'c' && videoWriter.isOpened())
		{
			videoWriter.release();
			remove(videoFilename.c_str());
		}
		else if (keyWithoutModifiers == 's' && videoWriter.isOpened())
		{
			frame.copyTo(frameMarkedForLabeling);
			frameNumberMarkedForLabeling = numFramesRecorded - 1;
		}
		else if (key == KEY_PAGE_UP || key == KEY_PAGE_DOWN)
		{
			try
			{
				p_acquisitionControl->exposureTime.write(p_acquisitionControl->exposureTime.read() + 100 * (key == KEY_PAGE_UP ? 1 : -1));
			}
			catch (const std::exception&)
			{
			}
		}
		else if (keyWithoutModifiers == '+' || keyWithoutModifiers == '-')
		{
			try
			{
				p_analogControl->gain.write(p_analogControl->gain.read() + 0.25 * (keyWithoutModifiers == '+' ? 1 : -1));
			}
			catch (const std::exception&)
			{
			}
		}

		// A hack to detect when the window is closed.
		// Source: https://stackoverflow.com/questions/35003476/opencv-python-how-to-detect-if-a-window-is-closed
		if (cv::getWindowProperty(WINDOW_NAME, 0) == -1)
			break;
	}

	return 0;
}