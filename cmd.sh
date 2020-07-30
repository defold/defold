CD_DEFOLD_MOJAVE="echo \"#Shorthand for entering defold folder\nalias cd_defold='cd $PWD'\" >> ~/.bash_profile"
CD_DEFOLD_CATALINA="echo \"#Shorthand for entering defold folder\nalias cd_defold='cd $PWD'\" >> ~/.zshrc"

SHELL="./scripts/build.py shell --platform=$2 --package-path=./local_sdks/"

SHELL_INTRO="echo \"#Shorthand for Defold Build Shell Command\" >> ~/.bash_profile"

SHELL_MOJAVE_x86_64="echo \"alias shell_defold_x86_64='./scripts/build.py shell --platform=x86_64-darwin --package-path=./local_sdks/'\" >> ~/.bash_profile"
SHELL_MOJAVE__armv7="echo \"alias shell_defold_armv7='./scripts/build.py shell --platform=armv7-darwin --package-path=./local_sdks/'\" >> ~/.bash_profile"
SHELL_MOJAVE__arm64="echo \"alias shell_defold_arm64='./scripts/build.py shell --platform=arm64-darwin --package-path=./local_sdks/'\" >> ~/.bash_profile"

SHELL_CATALINA_x86_64="echo \"alias shell_defold_x86_64='./scripts/build.py shell --platform=x86_64-darwin --package-path=./local_sdks/'\" >> ~/.zshrc"
SHELL_CATALINA__armv7="echo \"alias shell_defold_armv7='./scripts/build.py shell --platform=armv7-darwin --package-path=./local_sdks/'\" >> ~/.zshrc"
SHELL_CATALINA__arm64="echo \"alias shell_defold_arm64='./scripts/build.py shell --platform=arm64-darwin --package-path=./local_sdks/'\" >> ~/.zshrc"

SETUP="sh setup_env.sh"
BUNDLE="sh bundle_editor.sh $2"

SUB_MODULE="sh ./scripts/submodule.sh config_in_waf $2 $3 $4 $5 $6"
WAF_CONF="(cd $3;PREFIX=\$DYNAMO_HOME waf configure --platform=$2)"

BUILD_ENGINE="sudo ./scripts/build.py build_engine --platform=x86_64-darwin --skip-bob-light --skip-docs --skip-tests -- --skip-build-tests"
BUILD_ENGINE_IOS_v7="sudo ./scripts/build.py build_engine --platform=armv7-darwin --skip-bob-light --skip-docs --skip-tests -- --skip-build-tests"
BUILD_ENGINE_IOS_64="sudo ./scripts/build.py build_engine --platform=arm64-darwin --skip-bob-light --skip-docs --skip-tests -- --skip-build-tests"
BUILD_BUILTIN="sudo ./scripts/build.py build_builtins"
BUILD_BOB="sudo ./scripts/build.py build_bob --skip-tests"

RUN_EDITOR="(cd editor/;lein run)"
BUILD_EDITOR="(cd editor/;lein init)"
EDITOR="(cd editor/;lein init;lein run)"

FORCE="sudo chmod -R 777 ./"
BUILD_MODULE="./scripts/submodule.sh x86_64-darwin $2 $3"

ENGINE_PATH="./tmp/dynamo_home/bin/$2/"
EDITOR_PATH="./editor/tmp/unpack/$2/bin/"

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
  -w | --waf )
    eval $WAF_CONF
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
  -s | --setup )
    eval $SETUP
    exit
    ;;
  -sh | --shell )
    eval $SHELL_INTRO
    case $2 in
      mojave)
        eval $SHELL_MOJAVE_x86_64
        eval $SHELL_MOJAVE__armv7
        eval $SHELL_MOJAVE__arm64
        ;;
      catalina)
        eval $SHELL_CATALINA_x86_64
        eval $SHELL_CATALINA__armv7
        eval $SHELL_CATALINA__arm64
        ;;
    esac
    exit
    ;;
  -b | --editor )
    eval $EDITOR
    exit
    ;;
  -cp| --copy )
    echo "--------------------------------------------------"
    echo "COPY TO EDITOR ..."
    cp -r "${ENGINE_PATH}" "${EDITOR_PATH}"
    echo "copied \nfrom ${GREEN}${ENGINE_PATH}${NC} \nto ${GREEN}${EDITOR_PATH}${NC}"
    echo "--------------------------------------------------"
    exit
    ;;
  -e | --engine )
    eval $BUILD_ENGINE
    exit
    ;;
  -ev7 | --engine_ios_v7 )
    eval $BUILD_ENGINE_IOS_v7
    exit
    ;;
  -e64 | --engine_ios_64 )
    eval $BUILD_ENGINE_IOS_64
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
    echo "sh cmd.sh --shell | -sh: for shell x86_64-darwin | armv7-darwin | arm64..."
    echo "sh cmd.sh --copy  | -cp: to copy <platform> dmengine -> editor"
    echo "sh cmd.sh --engine| -e : for building engine alone"
    echo "sh cmd.sh --engine_ios_v7 | -ev7 : for building iOS armv7 engine"
    echo "sh cmd.sh --engine_ios_64 | -e64 : for building iOS arm64 engine"
    echo "sh cmd.sh --editor| -e : for building editor + launch"
    echo "sh cmd.sh --misc  | -m : for building bob + builtin"
    echo "sh cmd.sh --full  | -F : to build engine/editor + launch"
    echo "sh cmd.sh --fast  | -f : to fast build part of dmengine at maximum of 4"
    echo "sh cmd.sh --waf   | -w : to waf configure with (armv7 | arm64 | x86_64)-darwin"
    echo "sh cmd.sh --force | -fo: enable submodule when 'Operation is not permitted'"
    echo "sh cmd.sh --run   | -r : for running editor"
    echo "sh cmd.sh --bundle| -B : for bundling editor into ./editor/release with given version"
    echo "__________________[SHORTHAND]___________________"
    echo "sh cmd.sh --cd_mojave      | -cdm : add defold path so you can just call: cd_defold"
    echo "sh cmd.sh --cd_catalina    | -cdc : add defold path so you can just call: cd_defold"
    echo "______________________________________________"
    echo "You can also run each script separately as :"
    echo $SETUP
    echo $SHELL
    echo $COPY
    echo $SUB_MODULE
    echo $BUILD_ENGINE
    echo $BUILD_ENGINE_IOS_v7
    echo $BUILD_ENGINE_IOS_64
    echo $EDITOR
    echo $BUILD_BOB
    echo $BUILD_BUILTIN
    echo $BUILD_EDITOR
    echo $RUN_EDITOR
    echo $BUNDLE
    ;;
esac; shift; done
if [[ "$1" == '--' ]]; then shift; fi