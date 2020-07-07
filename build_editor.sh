SHELL="./scripts/build.py shell --platform=x86_64-darwin --package-path=./local_sdks/"

ENGINE="./scripts/build.py build_engine --platform=x86_64-darwin --skip-tests -- --skip-build-tests"

BUILTIN="sudo ./scripts/build.py build_builtins"

BOB="sudo ./scripts/build.py build_bob --skip-tests"

eval $SHELL
eval $ENGINE
eval $BUILTIN
eval $BOB

echo "Finish building editor. 'sh run_editor.sh' to run it."
