#!/bin/sh

set -e
export DYNAMO_HOME=/tmp/builder/dynamo-home

export PATH=$DYNAMO_HOME/bin:$PATH
export PATH=$DYNAMO_HOME/ext/bin:$DYNAMO_HOME/ext/bin/linux:$PATH
export LD_LIBRARY_PATH=$DYNAMO_HOME/lib:$DYNAMO_HOME/ext/lib/linux
export PYTHONPATH=$DYNAMO_HOME/lib/python:$DYNAMO_HOME/ext/lib/python

rm -rf pde_build
mkdir  pde_build
cd pde_build
PROJECTS="com.dynamo.cr.common com.dynamo.cr.editor com.dynamo.cr.luaeditor"

for P in $PROJECTS; do
    git clone overrated.dyndns.org:/repo/$P
done
cd ..

java -jar ~/eclipse/plugins/org.eclipse.equinox.launcher_1.1.0.v20100507.jar -application org.eclipse.ant.core.antRunner build.xml

CR=/var/www/cr/1.0

if [ $CR/repository -ef $CR/repository-a ]; then
    rm -rf $CR/repository-b
    mv /tmp/pde_build/repository $CR/repository-b
    ln -sfn $CR/repository-b $CR/repository
else
    rm -rf $CR/repository-a
    mv /tmp/pde_build/repository $CR/repository-a
    ln -sfn $CR/repository-a $CR/repository
fi

cp /tmp/pde_build/I.Editor/*.zip $CR
