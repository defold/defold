cgc -q -oglsl -profile arbfp1 %1 >NUL
if %errorlevel% GTR 0 exit %errorlevel%
cp %1 %2
