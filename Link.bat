@ECHO OFF
FOR %%f IN (%1) DO IF NOT EXIST %2\%%~nxf MKLINK /H %2\%%~nxf %%f