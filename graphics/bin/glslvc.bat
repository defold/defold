cgc -q -oglsl -profile arbvp1 %1 >NUL
if %errorlevel% GTR 0 exit %errorlevel%
cp %1 %2
