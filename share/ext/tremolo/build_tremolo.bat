echo off

rem Use the MSVS developer command prompt:
rem     https://learn.microsoft.com/en-us/cpp/build/building-on-the-command-line?view=msvc-170#developer_command_prompt_shortcuts

set PLATFORM=%1

set REPO_URL=https://github.com/defold/defold-tremolo.git

git clone %REPO_URL% tmp

pushd tmp

call build.bat %PLATFORM% ..\install

rem get git version
FOR /F "tokens=*" %%g IN ('git rev-parse --short HEAD') do (SET VERSION=%%g)

popd

rem Tar the bundle

echo "Using VERSION=%VERSION%"

mkdir ..\build

dir
echo "HELLO"

pushd .\install

dir
echo "HELLO2=%PREFIX%"

tar czvf ..\..\build\tremolo-%VERSION%-%PLATFORM%.tar.gz bin share lib
tar czvf ..\..\build\tremolo-%VERSION%-common.tar.gz include

popd

dir ..\build

rmdir /S /Q .\tmp
rmdir /S /Q .\install
