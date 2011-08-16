cgc -q -ogles -profile arbfp1 %1 >NUL
if %errorlevel% GTR 0 exit %errorlevel%
echo #ifdef GL_ES > %2
echo precision mediump float; >> %2
echo #endif >> %2
type %1 >> %2
