@echo off
set "DDFC=%~dp0ddfc.py"
rem Use the bundled dependencies on PYTHONPATH without importing host site-packages.
python -S "%DDFC%" --java %*
