@echo off

set PATH=%DYNAMO_EXT%\bin;%PATH%
set PATH=%DYNAMO_EXT%\bin\win32;%PATH%
set PYTHONPATH=%DYNAMO_EXT%/lib/python

call "%VS90COMNTOOLS%\vsvars32.bat"

"C:\Program Files\Git\bin\sh.exe" --login -i

