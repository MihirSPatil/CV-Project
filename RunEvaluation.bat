@ECHO OFF
IF NOT EXIST x64\Release\Evaluation.exe GOTO ERROR
IF NOT EXIST x64\Release\Template.exe GOTO ERROR
CALL CopyDLLs.bat
x64\Release\Evaluation.exe Template-C++ x64\Release\Template.exe Template-Python Projects\Template-Python\Template.py Data\Videos\Evaluation Results
IF ERRORLEVEL 1 PAUSE
EXIT /B 0
:ERROR
ECHO Please open the "CVC1819.sln" file, switch to "Release" mode and compile the projects "Evaluation" and "Template"!
PAUSE
EXIT /B 1