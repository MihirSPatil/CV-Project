#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import platform
import sys

architecture = platform.architecture()
if architecture[1] == "WindowsPE" and architecture[0] != "64bit":
    sys.stderr.write("The software package is targeted at 64-bit Python running on 64-bit Windows!\n")
    sys.exit(1)

# Make sure that NumPy is installed.
try:
    import numpy
except:
    sys.stderr.write("The NumPy package is required!\n")
    if architecture819 == "WindowsPE":
        sys.stderr.write("Launch the command prompt with administrator privileges, change to your Python installation directory (e.g. \"CD C:\\Python27\") and enter: \"Scripts\\pip install numpy\". If you don't have the \"pip\" tool yet (Python's package manager), install it by entering: \"python -m ensurepip --upgrade\".\n")
    else:
        sys.stderr.write("Install NumPy by entering into a terminal: \"sudo pip install numpy\"\n")
    sys.exit(1)

if architecture[1] == "WindowsPE":
    # Tell Python where to find the OpenCV DLLs.
    sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../x64/Release"))

# Import the OpenCV Python module.
import cv2

# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
# ! You should not change anything ABOVE this point. !
# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

# TODO: Implement this function!
def detect_dice(video_capture):
    # TODO: Detect the dice in the video provided! Return the result like shown here:
    
    # Zero-based number of the video frame that should be shown when displaying the detection results (one of the frames that you used for dice detection).
    reference_frame_no = 42

    # For each die detected, a tuple of (x, y, value).
    # "x", "y" is some pixel position within the die where it is visible (i.e. not occluded).
    # "value" is the number of dots shown by the die's top face (1 to 6).
    detected_dice = [(100, 200, 3), (400, 500, 6)]
    
    return reference_frame_no, detected_dice

# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
# ! You should not change anything BELOW this point. !
# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

if len(sys.argv) != 3:
    sys.stderr.write("Invalid command line arguments: Specify the video filename and the detection result filename!\n")
    sys.exit(1)

video_filename = sys.argv[1]
detection_result_filename = sys.argv[2]

video_capture = cv2.VideoCapture(video_filename)
if not video_capture.isOpened():
    sys.stderr.write("Failed to open video file\"" + video_filename + "\"!\n")
    sys.exit(1)

reference_frame_no, detected_dice = detect_dice(video_capture)

try:
    with open(detection_result_filename, "w") as file:
        file.write("%d\n" % reference_frame_no)
        file.write("%d\n" % len(detected_dice))
        for detected_die in detected_dice:
            file.write("%d %d %d\n" % detected_die)
except IOError:
    sys.stderr.write("Failed to open/create/write detection result file \"" + detection_result_filename + "\"!\n")
    sys.exit(1)

# When running from IDLE, any OpenCV windows will remain open.
cv2.destroyAllWindows()
