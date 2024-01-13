@echo off

set REL_PATH=%~dp0\..\Eagle-Editor
set ABS_PATH=

rem // Save current directory and change to target directory
pushd %REL_PATH%

rem // Save value of CD variable (current directory)
set ABS_PATH=%CD%\Eagle-Editor.exe

rem // Restore original directory
popd

ftype eagleeditor="%ABS_PATH%" "--project=%%1"
assoc .egproj=eagleeditor

PAUSE
