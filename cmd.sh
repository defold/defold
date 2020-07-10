SHELL_MOJAVE="echo 'alias shell_defold='./scripts/build.py shell --platform=x86_64-darwin --package-path=./local_sdks/' >> ~/.bash_profile"
SHELL_CATALINA="echo 'alias shell_defold='./scripts/build.py shell --platform=x86_64-darwin --package-path=./local_sdks/' >> ~/.zbashrc"
RUN="sh run_editor.sh"
SETUP="sh setup_env.sh"
BUILD="sh build_editor.sh"
BUNDLE="sh bundle_editor.sh"
BUILD_ENGINE="./scripts/build.py build_engine --platform=x86_64-darwin --skip-tests -- --skip-build-tests"
BUILD_BUILTIN="sudo ./scripts/build.py build_builtins"
BUILD_BOB="sudo ./scripts/build.py build_bob --skip-tests"


while [[ "$1" =~ ^- && ! "$1" == "--" ]]; do case $1 in
  -SM | --shell_mojave )
    eval $SHELL_MOJAVE
    exit
    ;;
  -SC | --shell_catalina )
    eval $SHELL_CATALINA
    exit
    ;;
  -s | --setup )
    eval $SETUP
    exit
    ;;
  -b | --build )
    eval $BUILD
    exit
    ;;
  -e | --engine )
    eval $BUILD_ENGINE
    exit
    ;;
  -m | --misc )
    eval $BUILD_BOB
    eval $BUILD_BUILTIN
    exit
    ;;
  -B | --bundle )
    eval $BUNDLE
    exit
    ;;
  -r | --run )
    eval $RUN
    exit
    ;;
  -h | --help )
    echo "______________________________________________"
    echo "Shorthand to build Defold Engine"
    echo "@thetrung | July 2020"
    echo ""
    echo "__________________[COMMAND]___________________"
    echo "sh cmd.sh --setup | -s : for environment setup"
    echo "sh cmd.sh --engine| -e : for building engine alone"
    echo "sh cmd.sh --build | -b : for building editor + engine"
    echo "sh cmd.sh --misc  | -m : for building bob + builtin"
    echo "sh cmd.sh --run   | -r : for running editor"
    echo "sh cmd.sh --bundle| -B : for bundling editor into ./editor/release"
    echo "sh cmd.sh --shell_mojave | -sm : to add shell_defold to your bash on Mojave"
    echo "sh cmd.sh --shell_catalina | -sc : to add shell_defold to your bash on Catalina"
    echo "______________________________________________"
    echo "You can also run each script separately as :"
    echo $RUN
    echo $SETUP
    echo $BUILD
    echo $BUNDLE
    ;;
esac; shift; done
if [[ "$1" == '--' ]]; then shift; fi