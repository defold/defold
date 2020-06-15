mkdir -p build/default/src/gamesys/test/texture
yes | cp -rf src/gamesys/test/texture/valid_png.png build/default/src/gamesys/test/texture/valid_png.png
waf --skip-tests && ./build/default/src/gamesys/test/test_gamesys