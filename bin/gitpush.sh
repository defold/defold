if [ -f .git -o -d .git ]; then
  pushd ..
else
  pushd .
fi
for dir in bounce com.dynamo.cr ddf dlib dmedit dynamo engine gameobject gamesys graphics gui hid input jetbot lua particle physics render resource script sound tools
do
  echo -ne '### \033[33m'
  echo -n "$dir"
  echo -e '\033[0m ###'
  cd $dir
  git push
  if [ $? -ne 0 ]; then
    break
  fi
  cd ..
done
popd
