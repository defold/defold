CD_DEFOLD_MOJAVE="echo \"#Shorthand for entering defold folder\nalias cd_defold='cd $PWD'\" >> ~/.bash_profile"
CD_DEFOLD_CATALINA="echo \"#Shorthand for entering defold folder\nalias cd_defold='cd $PWD'\" >> ~/.zshrc"

SHELL_MOJAVE="echo \"#Shorthand for Defold Build Shell Command\nalias shell_defold='./scripts/build.py shell --platform=x86_64-darwin --package-path=./local_sdks/'\" >> ~/.bash_profile"
SHELL_CATALINA="echo \"#Shorthand for Defold Build Shell Command\nalias shell_defold='./scripts/build.py shell --platform=x86_64-darwin --package-path=./local_sdks/'\" >> ~/.zshrc"

SETUP="sh setup_env.sh"
BUNDLE="sh bundle_editor.sh $2"

SUB_MODULE="sh ./scripts/submodule.sh x86_64-darwin $2 $3 $4 $5"
BUILD_ENGINE="sudo ./scripts/build.py build_engine --platform=x86_64-darwin --skip-tests -- --skip-build-tests"
BUILD_BUILTIN="sudo ./scripts/build.py build_builtins"
BUILD_BOB="sudo ./scripts/build.py build_bob --skip-tests"

RUN_EDITOR="(cd editor/;lein run)"
BUILD_EDITOR="(cd editor/;lein init)"
EDITOR="(cd editor/;lein init;lein run)"

FORCE="sudo chmod -R 777 ./"
BUILD_MODULE="./scripts/submodule.sh x86_64-darwin $2 $3"

ENGINE_PATH="./tmp/dynamo_home/bin/x86_64-darwin/*"
EDITOR_PATH="./editor/tmp/unpack/x86_64-darwin/bin/"

GREEN='\033[0;32m'
NC='\033[0m' # No Color

while [[ "$1" =~ ^- && ! "$1" == "--" ]]; do case $1 in
  -F | --full )
    start=$SECONDS
    eval $BUILD_ENGINE
    eval $BUILD_EDITOR
    duration=$(( SECONDS - start ))
    echo "====================================================="
    echo "${GREEN}Finished build in ${duration} secs. ${NC}"
    echo "====================================================="
    eval $RUN_EDITOR
    exit
    ;;
  -f | --fast )
    eval $SUB_MODULE
    exit
    ;;
  -fo | --force )
    eval $FORCE
    exit
    ;;
  -cdm | --cd_mojave )
    eval $CD_DEFOLD_MOJAVE
    exit
    ;;
  -cdc | --cd_catalina )
    eval $CD_DEFOLD_CATALINA
    exit
    ;;
  -sm | --shell_mojave )
    eval $SHELL_MOJAVE
    exit
    ;;
  -sc | --shell_catalina )
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
  -cp| --copy )
    echo "--------------------------------------------------"
    echo "COPY TO EDITOR ..."
    cp "${ENGINE_PATH}" "${EDITOR_PATH}"
    echo "copied \nfrom ${GREEN}${ENGINE_PATH}${NC} \nto ${GREEN}${EDITOR_PATH}${NC}"
    echo "--------------------------------------------------"
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
    start=$SECONDS
    eval $BUNDLE
    duration=$(( SECONDS - start ))
    echo "====================================================="
    echo "${GREEN}Finished bundling in ${duration} secs. ${NC}"
    echo "====================================================="
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
    echo "sh cmd.sh --copy  | -cp: to copy dmengine -> editor"
    echo "sh cmd.sh --engine| -e : for building engine alone"
    echo "sh cmd.sh --editor| -e : for building editor + launch"
    echo "sh cmd.sh --misc  | -m : for building bob + builtin"
    echo "sh cmd.sh --full  | -F : to build engine/editor + launch"
    echo "sh cmd.sh --fast  | -f : to fast build part of dmengine at maximum of 4"
    echo "sh cmd.sh --force | -fo: enable submodule when 'Operation is not permitted'"
    echo "sh cmd.sh --run   | -r : for running editor"
    echo "sh cmd.sh --bundle| -B : for bundling editor into ./editor/release with given version"
    echo "__________________[SHORTHAND]___________________"
    echo "sh cmd.sh --shell_mojave   | -sm : to run shell_defold on Mojave"
    echo "sh cmd.sh --shell_catalina | -sc : to run shell_defold on Catalina"
    echo "sh cmd.sh --cd_mojave      | -cdm : add defold path so you can just call: cd_defold"
    echo "sh cmd.sh --cd_catalina    | -cdc : add defold path so you can just call: cd_defold"
    echo "______________________________________________"
    echo "You can also run each script separately as :"
    echo $SETUP
    echo $SUB_MODULE
    echo $BUILD_ENGINE
    echo $BUILD_BOB
    echo $BUILD_BUILTIN
    echo $BUILD_EDITOR
    echo $RUN_EDITOR
    echo $BUNDLE
    ;;
esac; shift; done
if [[ "$1" == '--' ]]; then shift; fi