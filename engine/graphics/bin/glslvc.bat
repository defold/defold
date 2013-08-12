echo #ifndef GL_ES > $2
echo #define lowp >> $2
echo #define mediump >> $2
echo #define highp >> $2
echo #endif >> $2
type %1 >> %2
rem cgc -q -oglsl -profile arbvp1 %2 >NUL
if %errorlevel% GTR 0 exit %errorlevel%
