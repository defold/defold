Editor HTTP API
===============

External tools can connect to the running editor and interact with it using a REST API.

> [!IMPORTANT]  
> This HTTP API is currently in an experimental state. It might change drastically or even be removed completely if we deem it necessary. But hopefully, it shouldn't come to that.

### Overview
The editor can be interacted with by performing GET requests to the following URLs:
```
http://localhost:[port]/command
http://localhost:[port]/console
```

#### Port Configuration
Currently, the port is assigned randomly from a broad subrange. Since 1.11.0, the port is written to the `.internal/editor.port` file.

Additionally, since 1.11.0, the editor executable has a command line option `--port` (or `-p`), which allows specifying the port during launch, e.g.:
```bash
# on Windows
.\Defold.exe --port 8181

# on Linux:
./Defold --port 8181

# on macOS:
./Defold.app/Contents/MacOS/Defold --port 8181
```

### The Command Endpoint
The command endpoint exposes a subset of the commands available from the editor menu bar. To trigger a command, perform an empty POST request to a URL in the format:
```
http://localhost:[port]/command/[command]
```
You can obtain a list of the available commands along with a short description in JSON format by making a GET request to `http://localhost:[port]/command`. At the time of writing, this returns:
```json
{
  "asset-portal"             : "Open the Asset Portal in a web browser.",
  "build"                    : "Build and run the project.",
  "build-html5"              : "Build the project for HTML5 and open it in a web browser.",
  "debugger-break"           : "Break into the debugger.",
  "debugger-continue"        : "Resume execution in the debugger.",
  "debugger-detach"          : "Detach the debugger from the running project.",
  "debugger-start"           : "Start the project with the debugger, or attach the debugger to the running project.",
  "debugger-step-into"       : "Step into the current expression in the debugger.",
  "debugger-step-out"        : "Step out of the current expression in the debugger.",
  "debugger-step-over"       : "Step over the current expression in the debugger.",
  "debugger-stop"            : "Stop the debugger and the running project.",
  "documentation"            : "Open the Defold documentation in a web browser.",
  "donate-page"              : "Open the Donate to Defold page in a web browser.",
  "editor-logs"              : "Show the directory containing the editor logs.",
  "engine-profiler"          : "Open the Engine Profiler in a web browser.",
  "engine-resource-profiler" : "Open the Engine Resource Profiler in a web browser.",
  "fetch-libraries"          : "Download the latest version of the project library dependencies.",
  "hot-reload"               : "Hot-reload all modified files into the running project.",
  "issues"                   : "Open the Defold Issue Tracker in a web browser.",
  "rebuild"                  : "Rebuild and run the project.",
  "rebundle"                 : "Re-bundle the project using the previous Bundle dialog settings.",
  "reload-extensions"        : "Reload editor extensions.",
  "reload-stylesheets"       : "Reload editor stylesheets.",
  "report-issue"             : "Open the Report Issue page in a web browser.",
  "report-suggestion"        : "Open the Report Suggestion page in a web browser.",
  "show-build-errors"        : "Show the Build Errors tab.",
  "show-console"             : "Show the Console tab.",
  "show-curve-editor"        : "Show the Curve Editor tab.",
  "support-forum"            : "Open the Defold Support Forum in a web browser.",
  "toggle-pane-bottom"       : "Toggle visibility of the bottom editor pane.",
  "toggle-pane-left"         : "Toggle visibility of the left editor pane.",
  "toggle-pane-right"        : "Toggle visibility of the right editor pane."
}
```

#### Response Codes
Commands are fire-and-forget. You will not know if a command succeeds or not, only if your request was accepted. As such, all command requests will return the same response codes. They are as follows:

* `202 Accepted` - The request was accepted and we are processing it. We use this instead of `200 OK` since we won't be able to tell you when it is done or how it went.
* `400 Bad Request` - The request was malformed.
* `403 Forbidden` - The command is supported, but it can't be executed right now (for example if a build is already in progress).
* `404 Not Found` - The string after `command/` is not a supported command.
* `405 Method Not Allowed` - The request method is invalid. For example, trying to GET a command or POST to the console.
* `500 Internal Server Error` - Something unexpected went wrong when executing the command. The error will be logged in the editor log.

### The Console Endpoint
The console endpoint allows external tools to obtain the current contents of the Console View in the editor. Send a GET request to `http://localhost:[port]/console` to obtain the console contents in JSON format. The returned object contains `lines` and `regions`.

* The `lines` are returned as an array of strings, each string representing a line in the Console.
* The `regions` contain metadata about sub-regions of the `lines` that can inform formatting, etc.

#### Region Objects
Each region object will be in the following format:
```json
{
  "from": {"row": 0, "col": 0},
  "to":   {"row": 0, "col": 42},
  "type": "repeat"
}
```
* `from` and `to` specify the row and columns that encompass the region as zero-based indices into the `lines`.
* `type` is the type of the region, and can inform formatting.

#### Region Types
Depending on the `type` of a region, it might have additional fields. Here is a non-exhaustive list of region types used in the editor.

* `eval-expression` - An expression posted from the Evaluate Lua field in the debugger.
* `eval-result` - The result returned by the `eval-expression`.
* `eval-error` - A runtime error resulting from an `eval-expression`.
* `extension-output` - Output from an editor script or extension.
* `extension-error` - Errors from an editor script or extension.
* `repeat` - Repeated console output that has been collapsed. Its `count` is the number of repeats.
* `resource-reference` - A substring that matches an existing path in the project. It has a `proj-path-candidates` array of project-relative paths to matching resources, and can also have a zero-based `row` index into the resource if it is text-based.

In particular, `resource-reference` regions could be made clickable to allow the user to  navigate to the source of a Lua error as the editor will attempt to parse this information from callstacks.
