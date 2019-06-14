@echo off
call build.bat
copy preflight.exe ..\editor
cd ..\editor
call preflight.exe
cd ..\editor-preflight
