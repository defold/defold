@echo off
call lein uberjar
rmdir /s /q build-native-image 1> NUL 2> NUL
mkdir build-native-image
cd build-native-image
call native-image --static --initialize-at-build-time --report-unsupported-elements-at-runtime --no-fallback -cp ..\resources;..\target\uberjar\editor-preflight-0.0.1-SNAPSHOT-standalone.jar -J-Dclojure.spec.skip-macros=true -J-Dclojure.compiler.direct-linking=true -H:Name=preflight -H:+ReportExceptionStackTraces editor_preflight.core
call upx preflight.exe
copy preflight.exe ..
cd ..
