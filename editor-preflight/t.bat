@echo off
cd ..\editor
call java  -cp ..\editor-preflight\deps.jar;..\editor-preflight\src\;src\clj clojure.main -e "(in-ns 'user)(require '[editor-preflight.core :refer :all])(-main)"
cd ..\editor-preflight
