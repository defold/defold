function prebuild {

    [ -z $DYNAMO_HOME ] && echo "DYNAMO_HOME not set" && exit 1

    export BUILD_DIRECTORY=`pwd`/build

    if [ -z $BASE_LOCATION ]; then
        echo "warning: BASE_LOCATION not set, defaulting to $HOME/eclipse"
        export BASE_LOCATION=$HOME/eclipse
    fi

    export BUILD_PROPERTIES=`pwd`/$BUILD_PROPERTIES

    set -e
    os=`uname | awk '{ print tolower($0) }'`

    export PATH=$DYNAMO_HOME/bin:$PATH
    export PATH=$DYNAMO_HOME/ext/bin:$DYNAMO_HOME/ext/bin/${os}:$PATH
    export LD_LIBRARY_PATH=$DYNAMO_HOME/lib:$DYNAMO_HOME/ext/lib/${os}
    export PYTHONPATH=$DYNAMO_HOME/lib/python:$DYNAMO_HOME/ext/lib/python

    # NOTE: We clean manually here due to follow symlink problem in ant. Check build.xml
    rm -rf $BUILD_DIRECTORY

}

function gitclone {
    mkdir -p $BUILD_DIRECTORY/plugins
    mkdir -p $BUILD_DIRECTORY/features
    pushd $BUILD_DIRECTORY/plugins

    for P in $PROJECTS; do
        git clone overrated.dyndns.org:/repo/$P
    done
    popd
}

function build {
    java -jar $BASE_LOCATION/plugins/org.eclipse.equinox.launcher_1.1.0.v20100507.jar\
     -application org.eclipse.ant.core.antRunner\
     -buildfile $1\
     -DbaseLocation=$BASE_LOCATION\
     -DbuildDirectory=$BUILD_DIRECTORY\
     -DbuildProperties=$BUILD_PROPERTIES
}

function postbuild {
    if [ ! -z $DEPLOY_DIRECTORY ]; then

        if [ $DEPLOY_DIRECTORY/repository -ef $DEPLOY_DIRECTORY/repository-a ]; then
            rm -rf $DEPLOY_DIRECTORY/repository-b
            mv ${BUILD_DIRECTORY}/repository $DEPLOY_DIRECTORY/repository-b
            ln -sfn $DEPLOY_DIRECTORY/repository-b $DEPLOY_DIRECTORY/repository
        else
            rm -rf $DEPLOY_DIRECTORY/repository-a
            mv ${BUILD_DIRECTORY}/repository $DEPLOY_DIRECTORY/repository-a
            ln -sfn $DEPLOY_DIRECTORY/repository-a $DEPLOY_DIRECTORY/repository
        fi

        cp ${BUILD_DIRECTORY}/I.*/*.zip $DEPLOY_DIRECTORY
        chmod 775 -R $DEPLOY_DIRECTORY/*
    fi
}

