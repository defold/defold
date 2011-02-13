export CLASSPATH=ext/jetty-all-7.0.2.v20100331.jar:ext/servlet-api-2.5.jar:build/default/src/test/http_server:.

java TestHttpServer &
echo $! > test_http_server.pid

