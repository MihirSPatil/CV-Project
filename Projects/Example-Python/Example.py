#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import platform
import sys

base_directory = os.path.dirname(__file__) + "/../../"

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
    sys.path.insert(0, base_directory + "x64/Release")

# Import the OpenCV Python module.
import cv2

# Callback function for mouse interaction.
def mouseEvent(evt, x, y, flags, frame):
    if evt == cv2.EVENT_LBUTTONDOWN:
        # Get the pixel that was clicked and print its color.
        color = frame[y, x]
        red = color[2]
        green = color[1]
        blue = color[0]
        sys.stdout.write("Pixel at (%d, %d) has color (%d, %d, %d).\n" % (x, y, red, green, blue))

# Create an image of size 320x240 with 3 channels of 8-bit unsigned integers.
# Draw a red anti-aliased line of thickness 3 from (10, 10) to (200, 100).
# Draw a filled green ellipse with center (160, 120), width 100, height 50 and angle 20°.
# Then, display the image in a window.
image = numpy.zeros((240, 320, 3), numpy.uint8)
cv2.line(image, (10, 10), (200, 100), (0, 0, 255), 3, cv2.LINE_AA)
cv2.ellipse(image, ((160, 120), (100, 50), 20), (0, 255, 0), -1, cv2.LINE_AA)
cv2.imshow("Hello World", image)

# Load some image from a file.
koala_image = cv2.imread(base_directory + "Data/Example/Koala.jpg")

# Initialize video capture from camera and check if it worked. If not, use a video file.
video_capture = cv2.VideoCapture(0)
if video_capture.isOpened():
    print("Successfully opened a camera.")
    
    # Some webcams return a strange image the first time.
    # So we just read one frame and ignore it.
    video_capture.read()
else:
    print("Could not open camera! Opening a video file instead ...")
    video_capture = cv2.VideoCapture(base_directory + "Data/Example/Bunny.mp4")
    if not video_capture.isOpened():
        cv2.destroyAllWindows()
        sys.stderr.write("Could not open the video file either!\n")
        sys.exit(1)

# Create an image with the correct size for the video frames and 3 channels of 8-bit unsigned integers.
video_width = int(video_capture.get(cv2.CAP_PROP_FRAME_WIDTH))
video_height = int(video_capture.get(cv2.CAP_PROP_FRAME_HEIGHT))
print("Video frame size is %dx%d pixels." % (video_width, video_height))
frame = numpy.zeros((video_height, video_width, 3), numpy.uint8)
result, frame = video_capture.read()

# Create another window and give it a name.
window_name = "Video"
cv2.namedWindow(window_name)

# Set the mouse interaction callback function for the window.
# The image matrix will be passed as a parameter.
cv2.setMouseCallback(window_name, mouseEvent, frame)

# The main loop.
while True:
    # Read a video frame.
    # We abort when have reached the end of the video stream.
    result, new_frame = video_capture.read()
    if not result:
        break
    
    # Make sure the image is a 3-channel 24-bit image.
    if frame.dtype != "uint8" or frame.shape[2] != 3:
        sys.stderr.write("Unexpected image format!\n")
        sys.exit(1)

    # Copy the frame into our image.
    numpy.copyto(frame, new_frame)
    
    # Apply a 5x5 median filter.
    cv2.medianBlur(frame, 5, frame)
    
    # We will add the other image to our camera image.
    # If its size is not the same as the camera frame, resize it (this will only happen once).
    if koala_image.shape[:2] != frame.shape[:2]:
        koala_image = cv2.resize(koala_image, (frame.shape[1], frame.shape[0]), interpolation = cv2.INTER_CUBIC)
    cv2.addWeighted(frame, 0.75, koala_image, 0.25, 0, frame)
    
    # Display a text.
    cv2.putText(frame, "Click somewhere!", (50, 50), cv2.FONT_HERSHEY_PLAIN, 1.5, (255, 0, 255), 2)

    # Set the pixel at x = 40, y = 20 to red.
    frame[20, 40] = (0, 0, 255)
    
    # Show the image in the window.
    cv2.imshow(window_name, frame)

    # Quit the loop when the Esc key is pressed.
    # Calling waitKey is important, even if you're not interested in keyboard input!
    key_pressed = cv2.waitKey(1)
    if key_pressed != -1 and key_pressed != 255:
        # Only the least-significant 16 bits contain the actual key code. The other bits contain modifier key states.
        key_pressed &= 0xFFFF
        sys.stdout.write("Key pressed: %d\n" % key_pressed)
        if key_pressed == 27:
            break

print("That's it!")

# When running from IDLE, any OpenCV windows will remain open.
cv2.destroyAllWindows()
