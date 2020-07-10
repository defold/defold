SHELL_MOJAVE="echo 'alias shell_defold='./scripts/build.py shell --platform=x86_64-darwin --package-path=./local_sdks/' >> ~/.bash_profile"
SHELL_CATALINA="echo 'alias shell_defold='./scripts/build.py shell --platform=x86_64-darwin --package-path=./local_sdks/' >> ~/.zbashrc"

SETUP="sh setup_env.sh"
BUNDLE="sh bundle_editor.sh"

BUILD_ENGINE="sudo ./scripts/build.py build_engine --platform=x86_64-darwin --skip-tests -- --skip-build-tests"
BUILD_BUILTIN="sudo ./scripts/build.py build_builtins"
BUILD_BOB="sudo ./scripts/build.py build_bob --skip-tests"

RUN_EDITOR="(cd editor/;lein run)"
BUILD_EDITOR="(cd editor/;lein init)"
EDITOR="(cd editor/;lein init;lein run)"
BUILD_MODULE="./scripts/submodule.sh x86_64-darwin $2 $3"

CP_DME_1="cp ./tmp/dynamo_home/bin/x86_64-darwin/dmengine ./editor/tmp/unpack/x86_64-darwin/bin/dmengine"
CP_DME_2="cp ./tmp/dynamo_home/bin/x86_64-darwin/dmengine_release ./editor/tmp/unpack/x86_64-darwin/bin/dmengine_release"
CP_DME_3="cp ./tmp/dynamo_home/bin/x86_64-darwin/dmengine_headless ./editor/tmp/unpack/x86_64-darwin/bin/dmengine_headless"

GREEN='\033[0;32m'
NC='\033[0m' # No Color

while [[ "$1" =~ ^- && ! "$1" == "--" ]]; do case $1 in
  -F | --fast )
    start=$SECONDS
    eval $BUILD_ENGINE
    # eval $CP_DME_1
    # eval $CP_DME_2
    # eval $CP_DME_3
    eval $BUILD_EDITOR
    duration=$(( SECONDS - start ))
    echo "====================================================="
    echo "${GREEN}Finished build in ${duration} secs. ${NC}"
    echo "====================================================="
    eval $RUN_EDITOR
    exit
    ;;
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
  -b | --editor )
    eval $EDITOR
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
    eval $RUN_EDITOR
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
    echo "sh cmd.sh --editor| -e : for building editor + launch"
    echo "sh cmd.sh --misc  | -m : for building bob + builtin"
    
    echo "sh cmd.sh --fast  | -F : to build engine/editor + launch"
    
    
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