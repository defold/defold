[ ! -e $DYNAMO_HOME/share/doc ] && echo 'Unable to find $DYNAMO_HOME/share/doc' && exit 1
mkdir -p war/doc
mkdir -p war/doc/images
mkdir -p war/blog/images
mkdir -p war/site
cp -v $DYNAMO_HOME/share/doc/*doc*.json war/doc
cp -v $DYNAMO_HOME/share/doc/*.html war/site
# TODO: Remove this. Only here for backwards compatibility with old dynamic site
# The new static is mostly static and the documentation resides under /doc
cp -v -r $DYNAMO_HOME/share/doc/images war
cp -v -r $DYNAMO_HOME/share/doc/images war/doc
cp -v -r $DYNAMO_HOME/share/doc/images/blog war/blog/images
