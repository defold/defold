#!/bin/bash

# Wrapper script for maven
[ -z $DYNAMO_HOME ] && echo "DYNAMO_HOME not set" && exit 1

ln -sfn $DYNAMO_HOME ../com.dynamo.cr.common/DYNAMO_HOME
ln -sfn $DYNAMO_HOME ../com.dynamo.cr.contenteditor/DYNAMO_HOME

$DYNAMO_HOME/ext/share/maven/bin/mvn $@
