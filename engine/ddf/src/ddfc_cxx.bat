@echo off
set DDFC=%~dp0\ddfc.py
rem Use SDK dependencies from PYTHONPATH without mixing in system site packages.
python -S %DDFC% --cxx %*
