P=`pwd`
cd build/default/src/gamesys/test
python -m SimpleHTTPServer 6123 &
echo $! > $P/test_http_server.pid


