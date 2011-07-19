[ ! -e $DYNAMO_HOME/share/doc ] && echo 'Unable to find $DYNAMO_HOME/share/doc' && exit 1
mkdir -p war/doc
cp $DYNAMO_HOME/share/doc/*doc*.json war/doc

