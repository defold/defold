SHELL="./scripts/build.py shell --platform=x86_64-darwin --package-path=./local_sdks/"
RUN="sh run_editor.sh"
SETUP="sh setup_env.sh"
BUILD="sh build_editor.sh"
BUNDLE="sh bundle_editor.sh"

while [[ "$1" =~ ^- && ! "$1" == "--" ]]; do case $1 in
  -S | --shell )
    eval $SHELL
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
    echo "sh cmd.sh --build | -b : for building editor"
    echo "sh cmd.sh --run   | -r : for running editor"
    echo "sh cmd.sh --bundle| -B : for bundling editor"
    echo "______________________________________________"
    echo "You can also run each script separately as :"
    echo $RUN
    echo $SETUP
    echo $BUILD
    echo $BUNDLE
    ;;
esac; shift; done
if [[ "$1" == '--' ]]; then shift; fi