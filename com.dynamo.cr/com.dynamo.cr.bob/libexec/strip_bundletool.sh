#!/usr/bin/env bash

# Use this tool to remove certain items from bundletool-all.jar since we already include them in bob.jar

echo "Size at start"
ls -la bundletool-all.jar

zip -d bundletool-all.jar "windows/aapt2.exe" "linux/aapt2" "macos/aapt2"

echo "Removed aapt2"
ls -la bundletool-all.jar
