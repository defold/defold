@echo off

REM **********************************************************************
REM * compile.bat - MS Windows compilation batch file
REM *
REM * This is a "helper" script for the top-level Makefile for GLFW.
REM * It was introduced to eliminate incompability issues between
REM * Windows NT, 2000 and 9x (it's easier to make a script/makefile
REM * run accross different unices from different vendors than to make
REM * a script/makefile run across different Windows versions from
REM * Microsoft!).
REM *
REM * This batch file has been tested under Windows 98, NT 4.0 and 2k.
REM *
REM * Usage 1: compile MAKEPROG SUFFIX
REM *
REM * MAKEPROG  Name of make program (e.g. make or nmake)
REM * SUFFIX    Makefile suffix for a specific compiler (e.g. msvc)
REM *
REM * Usage 2: compile CLEAN
REM **********************************************************************

REM ----------------------------------------------------------------------
REM Check input arguments
REM ----------------------------------------------------------------------

IF %1 == CLEAN GOTO Cleanup
IF %1 == "" GOTO Error1
IF "%2" == "" GOTO Error1
IF NOT EXIST .\lib\win32\Makefile.win32.%2 GOTO Error2
GOTO ArgsOK

:Error1
echo *************************************************************************
echo ***       NOTE: THIS PROGRAM IS USED BY THE TOP LEVEL MAKEFILE.       ***
echo *** PLEASE READ 'README.HTML' FOR INFORMATION ON HOW TO COMPILE GLFW! ***
echo *************************************************************************
echo Usage 1: %0 MAKEPROG SUFFIX
echo  MAKEPROG - Name of make program (e.g. make or nmake)
echo  SUFFIX   - Makefile suffix for a specific compiler (e.g. mgw or msvc)
echo Usage 2: %0 CLEAN
goto End

:Error2
echo "%2" is not a vaild Makefile suffix
goto End

:ArgsOK

REM ----------------------------------------------------------------------
REM Build GLFW library (both static and dynamic, where supported)
REM ----------------------------------------------------------------------

cd .\lib\win32
%1 -f Makefile.win32.%2


REM ----------------------------------------------------------------------
REM Build example programs
REM ----------------------------------------------------------------------

cd ..\..\examples
%1 -f Makefile.win32.%2


REM ----------------------------------------------------------------------
REM Build test programs
REM ----------------------------------------------------------------------

cd ..\tests
%1 -f Makefile.win32.%2


REM ----------------------------------------------------------------------
REM Return to root directory
REM ----------------------------------------------------------------------

cd ..
GOTO End



REM ----------------------------------------------------------------------
REM Clean up compiled files
REM ----------------------------------------------------------------------

:Cleanup

REM Library object files
IF EXIST .\lib\win32\enable.o             del .\lib\win32\enable.o
IF EXIST .\lib\win32\fullscreen.o         del .\lib\win32\fullscreen.o
IF EXIST .\lib\win32\glext.o              del .\lib\win32\glext.o
IF EXIST .\lib\win32\image.o              del .\lib\win32\image.o
IF EXIST .\lib\win32\init.o               del .\lib\win32\init.o
IF EXIST .\lib\win32\input.o              del .\lib\win32\input.o
IF EXIST .\lib\win32\joystick.o           del .\lib\win32\joystick.o
IF EXIST .\lib\win32\stream.o             del .\lib\win32\stream.o
IF EXIST .\lib\win32\tga.o                del .\lib\win32\tga.o
IF EXIST .\lib\win32\thread.o             del .\lib\win32\thread.o
IF EXIST .\lib\win32\time.o               del .\lib\win32\time.o
IF EXIST .\lib\win32\window.o             del .\lib\win32\window.o
IF EXIST .\lib\win32\win32_enable.o       del .\lib\win32\win32_enable.o
IF EXIST .\lib\win32\win32_fullscreen.o   del .\lib\win32\win32_fullscreen.o
IF EXIST .\lib\win32\win32_glext.o        del .\lib\win32\win32_glext.o
IF EXIST .\lib\win32\win32_init.o         del .\lib\win32\win32_init.o
IF EXIST .\lib\win32\win32_joystick.o     del .\lib\win32\win32_joystick.o
IF EXIST .\lib\win32\win32_thread.o       del .\lib\win32\win32_thread.o
IF EXIST .\lib\win32\win32_time.o         del .\lib\win32\win32_time.o
IF EXIST .\lib\win32\win32_window.o       del .\lib\win32\win32_window.o

IF EXIST .\lib\win32\enable_dll.o         del .\lib\win32\enable_dll.o
IF EXIST .\lib\win32\fullscreen_dll.o     del .\lib\win32\fullscreen_dll.o
IF EXIST .\lib\win32\glext_dll.o          del .\lib\win32\glext_dll.o
IF EXIST .\lib\win32\image_dll.o          del .\lib\win32\image_dll.o
IF EXIST .\lib\win32\init_dll.o           del .\lib\win32\init_dll.o
IF EXIST .\lib\win32\input_dll.o          del .\lib\win32\input_dll.o
IF EXIST .\lib\win32\joystick_dll.o       del .\lib\win32\joystick_dll.o
IF EXIST .\lib\win32\stream_dll.o         del .\lib\win32\stream_dll.o
IF EXIST .\lib\win32\tga_dll.o            del .\lib\win32\tga_dll.o
IF EXIST .\lib\win32\thread_dll.o         del .\lib\win32\thread_dll.o
IF EXIST .\lib\win32\time_dll.o           del .\lib\win32\time_dll.o
IF EXIST .\lib\win32\window_dll.o         del .\lib\win32\window_dll.o
IF EXIST .\lib\win32\win32_dllmain_dll.o    del .\lib\win32\win32_dllmain_dll.o
IF EXIST .\lib\win32\win32_enable_dll.o     del .\lib\win32\win32_enable_dll.o
IF EXIST .\lib\win32\win32_fullscreen_dll.o del .\lib\win32\win32_fullscreen_dll.o
IF EXIST .\lib\win32\win32_glext_dll.o      del .\lib\win32\win32_glext_dll.o
IF EXIST .\lib\win32\win32_init_dll.o       del .\lib\win32\win32_init_dll.o
IF EXIST .\lib\win32\win32_joystick_dll.o   del .\lib\win32\win32_joystick_dll.o
IF EXIST .\lib\win32\win32_thread_dll.o     del .\lib\win32\win32_thread_dll.o
IF EXIST .\lib\win32\win32_time_dll.o       del .\lib\win32\win32_time_dll.o
IF EXIST .\lib\win32\win32_window_dll.o     del .\lib\win32\win32_window_dll.o

IF EXIST .\lib\win32\enable.obj           del .\lib\win32\enable.obj
IF EXIST .\lib\win32\fullscreen.obj       del .\lib\win32\fullscreen.obj
IF EXIST .\lib\win32\glext.obj            del .\lib\win32\glext.obj
IF EXIST .\lib\win32\image.obj            del .\lib\win32\image.obj
IF EXIST .\lib\win32\init.obj             del .\lib\win32\init.obj
IF EXIST .\lib\win32\input.obj            del .\lib\win32\input.obj
IF EXIST .\lib\win32\joystick.obj         del .\lib\win32\joystick.obj
IF EXIST .\lib\win32\stream.obj           del .\lib\win32\stream.obj
IF EXIST .\lib\win32\tga.obj              del .\lib\win32\tga.obj
IF EXIST .\lib\win32\thread.obj           del .\lib\win32\thread.obj
IF EXIST .\lib\win32\time.obj             del .\lib\win32\time.obj
IF EXIST .\lib\win32\window.obj           del .\lib\win32\window.obj
IF EXIST .\lib\win32\win32_enable.obj     del .\lib\win32\win32_enable.obj
IF EXIST .\lib\win32\win32_fullscreen.obj del .\lib\win32\win32_fullscreen.obj
IF EXIST .\lib\win32\win32_glext.obj      del .\lib\win32\win32_glext.obj
IF EXIST .\lib\win32\win32_init.obj       del .\lib\win32\win32_init.obj
IF EXIST .\lib\win32\win32_joystick.obj   del .\lib\win32\win32_joystick.obj
IF EXIST .\lib\win32\win32_thread.obj     del .\lib\win32\win32_thread.obj
IF EXIST .\lib\win32\win32_time.obj       del .\lib\win32\win32_time.obj
IF EXIST .\lib\win32\win32_window.obj     del .\lib\win32\win32_window.obj

IF EXIST .\lib\win32\enable_dll.obj       del .\lib\win32\enable_dll.obj
IF EXIST .\lib\win32\fullscreen_dll.obj   del .\lib\win32\fullscreen_dll.obj
IF EXIST .\lib\win32\glext_dll.obj        del .\lib\win32\glext_dll.obj
IF EXIST .\lib\win32\image_dll.obj        del .\lib\win32\image_dll.obj
IF EXIST .\lib\win32\init_dll.obj         del .\lib\win32\init_dll.obj
IF EXIST .\lib\win32\input_dll.obj        del .\lib\win32\input_dll.obj
IF EXIST .\lib\win32\joystick_dll.obj     del .\lib\win32\joystick_dll.obj
IF EXIST .\lib\win32\stream_dll.obj       del .\lib\win32\stream_dll.obj
IF EXIST .\lib\win32\tga_dll.obj          del .\lib\win32\tga_dll.obj
IF EXIST .\lib\win32\thread_dll.obj       del .\lib\win32\thread_dll.obj
IF EXIST .\lib\win32\time_dll.obj         del .\lib\win32\time_dll.obj
IF EXIST .\lib\win32\window_dll.obj       del .\lib\win32\window_dll.obj
IF EXIST .\lib\win32\win32_dllmain_dll.obj    del .\lib\win32\win32_dllmain_dll.obj
IF EXIST .\lib\win32\win32_enable_dll.obj     del .\lib\win32\win32_enable_dll.obj
IF EXIST .\lib\win32\win32_fullscreen_dll.obj del .\lib\win32\win32_fullscreen_dll.obj
IF EXIST .\lib\win32\win32_glext_dll.obj      del .\lib\win32\win32_glext_dll.obj
IF EXIST .\lib\win32\win32_init_dll.obj       del .\lib\win32\win32_init_dll.obj
IF EXIST .\lib\win32\win32_joystick_dll.obj   del .\lib\win32\win32_joystick_dll.obj
IF EXIST .\lib\win32\win32_thread_dll.obj     del .\lib\win32\win32_thread_dll.obj
IF EXIST .\lib\win32\win32_time_dll.obj       del .\lib\win32\win32_time_dll.obj
IF EXIST .\lib\win32\win32_window_dll.obj     del .\lib\win32\win32_window_dll.obj

REM Library files
IF EXIST .\lib\win32\libglfw.a            del .\lib\win32\libglfw.a
IF EXIST .\lib\win32\libglfwdll.a         del .\lib\win32\libglfwdll.a
IF EXIST .\lib\win32\glfw.exp             del .\lib\win32\glfw.exp
IF EXIST .\lib\win32\glfwdll.exp          del .\lib\win32\glfwdll.exp
IF EXIST .\lib\win32\glfw.lib             del .\lib\win32\glfw.lib
IF EXIST .\lib\win32\glfwdll.lib          del .\lib\win32\glfwdll.lib
IF EXIST .\lib\win32\glfw.dll             del .\lib\win32\glfw.dll
IF EXIST .\lib\win32\glfw.tds             del .\lib\win32\glfw.tds
IF EXIST .\lib\win32\init.tds             del .\lib\win32\init.tds

REM Executables and related files
IF EXIST .\examples\boing.exe             del .\examples\boing.exe
IF EXIST .\examples\gears.exe             del .\examples\gears.exe
IF EXIST .\examples\listmodes.exe         del .\examples\listmodes.exe
IF EXIST .\examples\mipmaps.exe           del .\examples\mipmaps.exe
IF EXIST .\examples\mtbench.exe           del .\examples\mtbench.exe
IF EXIST .\examples\mthello.exe           del .\examples\mthello.exe
IF EXIST .\examples\particles.exe         del .\examples\particles.exe
IF EXIST .\examples\pong3d.exe            del .\examples\pong3d.exe
IF EXIST .\examples\splitview.exe         del .\examples\splitview.exe
IF EXIST .\examples\triangle.exe          del .\examples\triangle.exe
IF EXIST .\examples\wave.exe              del .\examples\wave.exe

IF EXIST .\examples\boing.obj             del .\examples\boing.obj
IF EXIST .\examples\gears.obj             del .\examples\gears.obj
IF EXIST .\examples\listmodes.obj         del .\examples\listmodes.obj
IF EXIST .\examples\mipmaps.obj           del .\examples\mipmaps.obj
IF EXIST .\examples\mtbench.obj           del .\examples\mtbench.obj
IF EXIST .\examples\mthello.obj           del .\examples\mthello.obj
IF EXIST .\examples\particles.obj         del .\examples\particles.obj
IF EXIST .\examples\pong3d.obj            del .\examples\pong3d.obj
IF EXIST .\examples\splitview.obj         del .\examples\splitview.obj
IF EXIST .\examples\triangle.obj          del .\examples\triangle.obj
IF EXIST .\examples\wave.obj              del .\examples\wave.obj

IF EXIST .\examples\boing.tds             del .\examples\boing.tds
IF EXIST .\examples\gears.tds             del .\examples\gears.tds
IF EXIST .\examples\listmodes.tds         del .\examples\listmodes.tds
IF EXIST .\examples\mipmaps.tds           del .\examples\mipmaps.tds
IF EXIST .\examples\mtbench.tds           del .\examples\mtbench.tds
IF EXIST .\examples\mthello.tds           del .\examples\mthello.tds
IF EXIST .\examples\particles.tds         del .\examples\particles.tds
IF EXIST .\examples\pong3d.tds            del .\examples\pong3d.tds
IF EXIST .\examples\splitview.tds         del .\examples\splitview.tds
IF EXIST .\examples\triangle.tds          del .\examples\triangle.tds
IF EXIST .\examples\wave.tds              del .\examples\wave.tds

IF EXIST .\tests\accuracy.exe             del .\tests\accuracy.exe
IF EXIST .\tests\defaults.exe             del .\tests\defaults.exe
IF EXIST .\tests\events.exe               del .\tests\events.exe
IF EXIST .\tests\fsaa.exe                 del .\tests\fsaa.exe
IF EXIST .\tests\fsinput.exe              del .\tests\fsinput.exe
IF EXIST .\tests\joysticks.exe            del .\tests\joysticks.exe
IF EXIST .\tests\peter.exe                del .\tests\peter.exe
IF EXIST .\tests\reopen.exe               del .\tests\reopen.exe
IF EXIST .\tests\tearing.exe              del .\tests\tearing.exe
IF EXIST .\tests\version.exe              del .\tests\version.exe

:End

