Editor HTTP API
===============

External tools can connect to the running editor and interact with it using a REST API.

> [!IMPORTANT]  
> This HTTP API is currently in an experimental state. It might change drastically or even be removed completely if we deem it necessary. But hopefully, it shouldn't come to that.

### Overview

The editor exposes a local HTTP server while a project is open.

To quickly discover the current port, run the editor command `Help → Open Editor Server`. This opens the local server page in your browser.

Interact with it by sending HTTP requests to:
```
http://localhost:[port]/[endpoint]
```

Endpoint descriptions are available in the OpenAPI ([v3.0.3](https://spec.openapis.org/oas/v3.0.3.html)) document:
```
http://localhost:[port]/openapi.json
```
A link to this document is displayed on the index page that opens when you invoke the `Help → Open Editor Server` command.

#### Port Configuration

By default, the port is assigned randomly from a broad subrange. It is written to the `.internal/editor.port` file.

Additionally, the editor executable has a command line option `--port` (or `-p`), which allows specifying the port during launch, e.g.:
```bash
# on Windows
.\Defold.exe --port 8181

# on Linux:
./Defold --port 8181

# on macOS:
./Defold.app/Contents/MacOS/Defold --port 8181
```

### OpenAPI

The OpenAPI ([v3.0.3](https://spec.openapis.org/oas/v3.0.3.html)) document is available from the local server index page via the `OpenAPI spec` link.

If needed, you can also request it directly at:
```
http://localhost:[port]/openapi.json
```

Use this document as the source of truth for the currently available API operations, including their request and response schemas.
