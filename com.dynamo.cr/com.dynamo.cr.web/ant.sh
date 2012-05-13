#!/bin/bash

# Wrapper script for maven
[ -z $DYNAMO_HOME ] && echo "DYNAMO_HOME not set" && exit 1
[ -z $ECLIPSE_HOME ] && echo "ECLIPSE_HOME not set" && exit 1

os=`uname | awk '{ print tolower($0) }'`

export PATH=$DYNAMO_HOME/bin:$PATH
export PATH=$DYNAMO_HOME/ext/bin:$DYNAMO_HOME/ext/bin/${os}:$PATH
export LD_LIBRARY_PATH=$DYNAMO_HOME/lib:$DYNAMO_HOME/ext/lib/${os}
export PYTHONPATH=$DYNAMO_HOME/lib/python:$DYNAMO_HOME/ext/lib/python

java -jar $ECLIPSE_HOME/plugins/org.eclipse.equinox.launcher_1.1.0.v20100507.jar -application org.eclipse.ant.core.antRunner -buildfile build.xml -DECLIPSE_HOME=$ECLIPSE_HOME $@
