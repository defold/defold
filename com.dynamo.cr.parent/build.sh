#!/bin/bash

# Wrapper script for maven
[ -z $DYNAMO_HOME ] && echo "DYNAMO_HOME not set" && exit 1
$DYNAMO_HOME/ext/share/maven/bin/mvn $@
