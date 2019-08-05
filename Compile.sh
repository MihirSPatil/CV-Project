#!/usr/bin/env bash

mkdir --parents x64/Release
command -v pkg-config >/dev/null 2>&1 || { echo >&2 '"pkg-config" is required. Please install it, maybe using "sudo apt-get install pkg-config".'; echo 'Press any key to continue.'; read -n 1 -s; exit 1; }
OPENCV_COMPILER_ARGS=$(pkg-config --cflags --libs opencv 2>/dev/null)
if [[ -z $OPENCV_COMPILER_ARGS ]]; then
	echo '"pkg-config" failed to find OpenCV. Please install it, maybe using "sudo apt-get install libopencv-dev python3-opencv".'
	echo 'Press any key to continue.'
	read -n 1 -s
	exit 1
fi
g++ -O3 Projects/Evaluation/Evaluation.cpp -I./Libraries/JSON -lstdc++fs $OPENCV_COMPILER_ARGS -o x64/Release/Evaluation
g++ -O3 Projects/Example-C++/Example.cpp -lstdc++fs $OPENCV_COMPILER_ARGS -o x64/Release/Example
g++ -O3 Projects/Template-C++/Template.cpp $OPENCV_COMPILER_ARGS -o x64/Release/Template
