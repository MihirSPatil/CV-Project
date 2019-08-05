@ECHO OFF

IF NOT EXIST x64 MKDIR x64

IF NOT EXIST x64\Debug MKDIR x64\Debug
CALL Link.bat Libraries\*.dll x64\Debug
CALL Link.bat Libraries\OpenCV\build\x64\vc15\bin\*.dll x64\Debug
CALL Link.bat Libraries\OpenCV\build\python\2.7\x64\*.pyd x64\Debug
CALL Link.bat Libraries\OpenCV\build\python\3.6\x64\*.pyd x64\Debug

IF NOT EXIST x64\Release MKDIR x64\Release
CALL Link.bat Libraries\*.dll x64\Release
CALL Link.bat Libraries\OpenCV\build\x64\vc15\bin\*.dll x64\Release
CALL Link.bat Libraries\OpenCV\build\python\2.7\x64\*.pyd x64\Release
CALL Link.bat Libraries\OpenCV\build\python\3.6\x64\*.pyd x64\Release