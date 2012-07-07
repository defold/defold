[ ! -e $DYNAMO_HOME/share/doc ] && echo 'Unable to find $DYNAMO_HOME/share/doc' && exit 1
mkdir -p war/doc
mkdir -p war/doc/images
mkdir -p war/blog/images
mkdir -p war/site
cp -v $DYNAMO_HOME/share/doc/*doc*.json war/doc
cp -v $DYNAMO_HOME/share/doc/*.html war/site
cp -v -r $DYNAMO_HOME/share/doc/images war/doc
cp -v -r $DYNAMO_HOME/share/doc/images/blog war/blog/images
