cgc -q -oglsl -profile arbfp1 %1 >NUL
if %errorlevel% GTR 0 exit %errorlevel%
copy %1 %2
