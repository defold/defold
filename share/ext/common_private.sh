# The existance of this

function cmi_private() {
    export PREFIX=`pwd`/build
    export PLATFORM=$1

    case $PLATFORM in
        arm64-nx64)
            echo "Has arm64-nx64 support"
            cmi_cross $PLATFORM $PLATFORM
            ;;

        *)
            echo "Unknown target $1" && exit 1
            ;;
    esac
}