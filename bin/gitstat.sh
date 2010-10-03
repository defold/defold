if [ -f .git ]; then
  pushd ..
else
  pushd .
fi
for dir in com.dynamo.cr.common com.dynamo.cr.editor com.dynamo.cr.server ddf dlib dmedit dynamo engine gameobject gamesys graphics gui hid input jetbot lua particle physics render resource script sound tools
do
  echo -ne '### \E[33m'
  echo -n "$dir"
  echo -e '\E[0m ###'
  cd $dir
  git branch
  git status -s
  cd ..
done
popd
