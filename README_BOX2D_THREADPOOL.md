# Box2D Thread Pool Setup
This document outlines the steps to set up a thread pool sessionb for Box2D in a wasm_pthread-web environment.

Build the engine with the following command:
```shell
./scripts/build.py build_engine --generate-compile-commands --skip-docs --skip-bob-light --skip-tests -- --skip-build-tests --platform=wasm_pthread-web
```

Build Bob in order to pick up the new engine
```shell
./scripts/build.py build_bob
```

Run bob to build the defold project, and bundle it:
```shell
java -jar ./tmp/dynamo_home/share/java/bob.jar --platform=wasm_pthread-web --architectures=wasm_pthread-web --archive resolve build bundle
```

Create a Python script to serve the files with the necessary headers for CORS and COOP/COEP. This is needed for pthread support.
```python
#!/usr/bin/env python3


import http.server
import socketserver
import threading

PORT = 8000

class Handler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        super().end_headers()

def run_server(httpd):
    print(f"Serving on http://localhost:{PORT}")
    httpd.serve_forever()
    print("Server has shut down.")

# Set up server
httpd = socketserver.TCPServer(('', PORT), Handler)

# Start in background thread
server_thread = threading.Thread(target=run_server, args=(httpd,))
server_thread.start()

try:
    input("Press ENTER to stop the server...\n")
except KeyboardInterrupt:
    pass

print("Stopping server...")
httpd.shutdown()
server_thread.join()
print("Server stopped.")
```

Save the above script as `serve.py` and execute it.
```shell
./serve.py
```

Control the number of threads in thread pool using _WORKER_COUNT_ in _box2d_physics.cpp_. 0 means running on main, any number greater than 0 will create a thread pool with that many threads.
```cpp
static const int WORKER_COUNT = 2;
```
