P=`pwd`
cd build/default/src/gamesys/test
python -m SimpleHTTPServer 6000 &
echo $! > $P/test_http_server.pid


