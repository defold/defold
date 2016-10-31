set -e
kill `cat test_http_server.pid`
rm test_http_server.pid
rm test_http_server.cfg
