echo #ifdef GL_ES > %2
echo precision mediump float; >> %2
echo #endif >> %2
echo #ifndef GL_ES >> %2
echo #define lowp >> %2
echo #define mediump >> %2
echo #define highp >> %2
echo #endif >> %2
type %1 >> %2
cgc -q -oglsl -profile arbfp1 %2 >NUL
if %errorlevel% GTR 0 exit %errorlevel%
