if [ -f .git -o -d .git ]; then
  pushd ..
else
  pushd .
fi
for dir in bounce com.dynamo.cr.common com.dynamo.cr.editor com.dynamo.cr.server ddf dlib dmedit dynamo engine gameobject gamesys graphics gui hid input jetbot lua particle physics render resource script sound tools
do
  echo -ne '### \E[33m'
  echo -n "$dir"
  echo -e '\E[0m ###'
  cd $dir
  git pull --rebase
  if [ $? -ne 0 ]; then
    break
  fi
  cd ..
done
popd
