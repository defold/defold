#!/usr/bin/env bash

DESKTOP_ENTRY_PATH=~/.local/share/applications/defold-editor-test.desktop
PWD=$(pwd)

touch $DESKTOP_ENTRY_PATH

{
  echo "[Desktop Entry]"
  echo "Name=Defold Editor TEST"
  echo "Comment=Run Defold Editor"
  echo "Terminal=false"
  echo "Type=Application"
  echo "StartupWMClass=com.defold.editor.Start"
  echo "Categories=Development;Editor;"
  echo "StartupNotify=true"
  echo "Exec=$PWD/Defold"
  echo "Icon=$PWD/logo_blue.png"
} > $DESKTOP_ENTRY_PATH
