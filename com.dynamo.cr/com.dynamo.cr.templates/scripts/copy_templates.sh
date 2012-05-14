set -e

[ ! -e templates ] && echo "Directory templates not found. Running script from other directory than root of template plug-in?" && exit 5
for d in `ls templates`; do
    [ -d "templates/$d" ] && rm -rf "templates/$d"
done

cp -r $DYNAMO_HOME/content/samples templates
cp -r $DYNAMO_HOME/content/tutorials templates
