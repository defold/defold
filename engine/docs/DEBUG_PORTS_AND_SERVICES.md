# Engine services and ports

It is possible to connect to and interact with a debug version of the engine through a number of different open TCP ports and services. The following services and ports are typically available in a debug build of the engine:

* SSDP - Port 1900.
* Engine service - Port 8001 or the port specified in `DM_SERVICE_PORT` environment variable. When running from the editor `DM_SERVICE_PORT` is set to “dynamic” which means that the engine will let the OS assign a random available port.
* Redirect service - Port 8002. 
* Log service - Port assigned by OS. 
* Remotery - Port 17815.


## SSDP
SSDP ([Simple Service Discovery Protocol](https://en.wikipedia.org/wiki/Simple_Service_Discovery_Protocol)) is used by the running engine to [broadcast](https://github.com/defold/defold/blob/dev/engine/dlib/src/dlib/ssdp.cpp) its existence on the network so that the editor can [discover](https://github.com/defold/defold/blob/dev/editor/src/java/com/dynamo/upnp/SSDP.java) it and connect to issue commands.

<details><summary>Example of a client in JavaScript</summary><p>

Using npm package [node-ssdp](https://www.npmjs.com/package/node-ssdp)

```js
const Client = require('node-ssdp').Client;
const client = new Client();

client.on('response', function (headers, statusCode, rinfo) {
	if (headers.SERVER.indexOf('Defold') !== -1) {
		console.log('Found running Defold Engine!', headers.LOCATION);
	}
});
client.search('upnp:rootdevice');
```

![image](https://user-images.githubusercontent.com/7230306/217389478-c99309f7-5bed-4412-bdeb-a6a0a5ab47b3.png)
 
</p></details>

## Engine service
The engine service is implemented as a small web server running within the engine. The server provides a number of endpoints which can be used to query for data or issue commands:

### /post
This endpoint will accept a POST request containing a protobuf command. One such example is the `Reload` command from `resource_ddf.proto` to reload a resource when hot-reloading content. Examples (also check `engine.clj` where some of these are used):

* `/post/@system/reboot` - Reboot the engine. `com.dynamo.system.proto.System$Reboot`
* `/post/@system/run_script` - Run a Lua script. `com.dynamo.engine.proto.Engine$RunScript`
* `/post/@render/resize` - Resize the engine window. `com.dynamo.render.proto.Render$Resize`
* `/post/@resource/reload` - Reload a resource (ie hot-reload). `com.dynamo.resource.proto.Resource$Reload`

### /ping
This endpoint will accept a GET request and reply with a "pong". This can be used to check that the server is running and responding to requests.

### /info
This endpoint will accept a GET request and reply with a JSON formatted string containing information about the engine:

```json
{"version": "1.4.1", "platform": "x86_64-macos", "sha1": "8f96e450ddfb006a99aa134fdd373cace3760571"}
```

### /upnp
This endpoint will accept a GET request and reply with an upnp specification (XML).


## Redirect service
The redirect service will redirect any request to the engine service on its actual port (see Engine Service)


## Log service
The log service can be used to read logs from the engine using a TCP socket.


## Remotery
Remotery, the high performance profiler used in Defold, serves data on port 17815 using a Web Socket connection.
