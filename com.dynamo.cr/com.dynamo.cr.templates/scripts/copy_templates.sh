#!/bin/bash

set -e

[ ! -e templates ] && echo "Directory templates not found. Running script from other directory than root of template plug-in?" && exit 5
rm -f templates/templates.zip

TEMPLATES_DIR=`pwd`/templates
pushd $DYNAMO_HOME/content >/dev/null
zip -r $TEMPLATES_DIR/templates samples tutorials
popd >/dev/null
