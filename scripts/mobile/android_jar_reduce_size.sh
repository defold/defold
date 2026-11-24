#!/bin/bash
# This script unpack jar file, replace all the pixels in all the PNG files with balck
# color and pack it again

if ! command -v convert &> /dev/null; then
    echo "ImageMagick is not installed on this Mac."
    echo "To install it, run the following command:"
    echo "brew install imagemagick"
    exit
fi

if [ -z "$1" ]
  then
    echo "Usage: $0 <path-to-jar-file>"
    exit 1
fi

JAR_PATH=$(readlink -f "$1")
TMP_DIR=$(mktemp -d)

echo "Unpacking jar file $JAR_PATH"
unzip -q "$JAR_PATH" -d "$TMP_DIR"

echo "Replacing pixels in PNG files..."
find "$TMP_DIR" -type f -name "*.png" -print0 | while IFS= read -r -d '' file
do
  magick "$file" -alpha opaque -fill black -colorize 100% "$file"
done

echo "Repacking jar file..."
cd "$TMP_DIR"
jar cf "$JAR_PATH" *
cd -

rm -rf "$TMP_DIR"

echo "Updated jar file: $JAR_PATH"

echo "Done!"
