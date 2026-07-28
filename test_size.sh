#!/usr/bin/env bash

set +e
rm test.wasm
rm test.tar
rm test.tar.br

set -e

cp -v ./tmp/dynamo_home/bin/wasm-web/dmengine.wasm test.wasm

ls -la test.wasm

tar -cvf test.tar ./test.wasm

ls -la test.tar

brotli -Z test.tar

ls -la test.tar.br

