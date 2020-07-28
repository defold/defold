ENGINE="./scripts/build.py build_engine --platform=x86_64-darwin --skip-tests -- --skip-build-tests"

BUILTIN="sudo ./scripts/build.py build_builtins"

BOB="sudo ./scripts/build.py build_bob --skip-tests"

LEIN_INIT="(cd ./editor;lein init)"

eval $ENGINE
eval $BUILTIN
eval $BOB
eval $LEIN_INIT

echo "Finish building editor. 'sh run_editor.sh' to run it."
echo "_____________________________________________________"