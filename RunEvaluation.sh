#!/usr/bin/env bash

if [[ ! -x Projects/Template-Python/Template.py ]]; then
	echo 'Please make the file "Projects/Template-Python/Template.py" executable, try executing "bash SetExecutableBits.sh"!'
	echo 'Press any key to continue.'
	read -n 1 -s
	exit 1
elif [[ ! -x x64/Release/Evaluation || ! -x x64/Release/Template ]]; then
	echo 'Please compile the "Evaluation" and "Template" programs, try executing "Compile.sh"!'
	echo 'Press any key to continue.'
	read -n 1 -s
	exit 1
fi
x64/Release/Evaluation Template-C++ x64/Release/Template Template-Python Projects/Template-Python/Template.py Data/Videos/Evaluation Results || { echo 'Press any key to continue.'; read -n 1 -s; exit 1; }
