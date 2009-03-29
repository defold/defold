@echo off

set PATH=%DYNAMO_HOME%\bin;%PATH%
set PATH=%DYNAMO_HOME%\ext\bin;%PATH%
set PATH=%DYNAMO_HOME%\ext\bin\win32;%PATH%
set PYTHONPATH=%DYNAMO_HOME%/lib/python;%DYNAMO_HOME%/ext/lib/python

call "%VS90COMNTOOLS%\vsvars32.bat"

"C:\Program Files\Git\bin\sh.exe" --login -i

