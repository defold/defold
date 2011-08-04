function prebuild {

    # Set stack limit to 8MB. When running from buildbot the stack limit is set to unlimited
    # for unknown reasons (8192 is the default in a login shell on dev.defold.com)
    # With stack set to unlimited we got memory exceptions when building editor with pde.
    # Specifically mmap and zip failed in java.
    # Could perhaps be due to heap/stack and virtual address space?
    ulimit -s 8192

    [ -z $DYNAMO_HOME ] && echo "DYNAMO_HOME not set" && exit 1

    export BUILD_DIRECTORY=`pwd`/build

    if [ -z $BASE_LOCATION ]; then
        echo "warning: BASE_LOCATION not set, defaulting to $HOME/eclipse"
        export BASE_LOCATION=$HOME/eclipse
    fi

    set -e
    os=`uname | awk '{ print tolower($0) }'`

    export PATH=$DYNAMO_HOME/bin:$PATH
    export PATH=$DYNAMO_HOME/ext/bin:$DYNAMO_HOME/ext/bin/${os}:$PATH
    export LD_LIBRARY_PATH=$DYNAMO_HOME/lib:$DYNAMO_HOME/ext/lib/${os}
    export PYTHONPATH=$DYNAMO_HOME/lib/python:$DYNAMO_HOME/ext/lib/python
}

function gitclone {
    # NOTE: We clean manually here due to follow symlink problem in ant. Check build.xml
    rm -rf $BUILD_DIRECTORY

    mkdir -p $BUILD_DIRECTORY/plugins
    mkdir -p $BUILD_DIRECTORY/features
    git clone overrated.dyndns.org:/repo/com.dynamo.cr $BUILD_DIRECTORY/plugins
}

function build {
    java -Xms256m -Xmx1024m -jar $BASE_LOCATION/plugins/org.eclipse.equinox.launcher_1.2.0.v20110502.jar\
     -application org.eclipse.ant.core.antRunner\
     -buildfile $1\
     -DbaseLocation=$BASE_LOCATION\
     -DbuildDirectory=$BUILD_DIRECTORY\
     -DbuildProperties=`pwd`/$2\
     -data /tmp/workspace_$3
}

function postbuild {
    # DEPLOY_PATHS is on format "host1:/path1 host2:/path2 ..."
    for DEPLOY_PATH in $DEPLOY_PATHS
    do
        TMP=$(echo $DEPLOY_PATH | tr ":" "\n")
        HOST_DIR=($TMP)
        local HOST=${HOST_DIR[0]}
        local DIR=${HOST_DIR[1]}

        tar -C ${BUILD_DIRECTORY} -cvz repository | ssh $HOST "rm -rf ${DIR}_tmp; mkdir -p ${DIR}_tmp; cd ${DIR}_tmp; tar xvfz -"
        ssh $HOST "rm -rf ${DIR}; mv ${DIR}_tmp ${DIR}"

        scp ${BUILD_DIRECTORY}/I.*/*.zip $DEPLOY_PATH
        ssh $HOST "chmod -R 775 ${DIR}"
    done
}

