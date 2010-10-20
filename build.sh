#!/bin/sh

[ -z $DYNAMO_HOME ] && echo "DYNAMO_HOME not set" && exit 1

if [ -z $BUILD_DIRECTORY ]; then
    echo "warning: BUILD_DIRECTORY not set, defaulting to /tmp/pde_build"
    export BUILD_DIRECTORY=/tmp/pde_build
fi

if [ -z $BASE_LOCATION ]; then
    echo "warning: BASE_LOCATION not set, defaulting to $HOME/eclipse"
    export BASE_LOCATION=$HOME/eclipse
fi

if [ -z $BUILD_PROPERTIES ]; then
    echo "warning: BUILD_PROPERTIES not set, defaulting to build.properties"
    export BUILD_PROPERTIES=build.properties
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

rm -rf projects
mkdir  projects
cd projects
PROJECTS="com.dynamo.cr.common com.dynamo.cr.editor com.dynamo.cr.contenteditor com.dynamo.cr.luaeditor com.dynamo.cr.server"

for P in $PROJECTS; do
    git clone overrated.dyndns.org:/repo/$P
done
cd ..

java -jar $BASE_LOCATION/plugins/org.eclipse.equinox.launcher_1.1.0.v20100507.jar\
 -application org.eclipse.ant.core.antRunner build.xml\
 -DbaseLocation=$BASE_LOCATION\
 -DbuildDirectory=$BUILD_DIRECTORY\
 -DbuildProperties=$BUILD_PROPERTIES

if [ ! -z $DEPLOY_DIRECTORY ]; then

    if [ $DEPLOY_DIRECTORY/repository -ef $DEPLOY_DIRECTORY/repository-a ]; then
        rm -rf $DEPLOY_DIRECTORY/repository-b
        mv /tmp/pde_build/repository $DEPLOY_DIRECTORY/repository-b
        ln -sfn $DEPLOY_DIRECTORY/repository-b $DEPLOY_DIRECTORY/repository
    else
        rm -rf $DEPLOY_DIRECTORY/repository-a
        mv /tmp/pde_build/repository $DEPLOY_DIRECTORY/repository-a
        ln -sfn $DEPLOY_DIRECTORY/repository-a $DEPLOY_DIRECTORY/repository
    fi

    cp /tmp/pde_build/I.Editor/*.zip $DEPLOY_DIRECTORY
    chmod 775 -R $DEPLOY_DIRECTORY/*
fi



