#!/usr/bin/env bash

set -e
if [ -f test_http_server.pid ]; then
	kill "$(cat test_http_server.pid)" 2>/dev/null || true
	rm -f test_http_server.pid
fi
rm -f test_http_server.cfg
