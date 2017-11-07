# defold-robot

Automatic smoke testing of software, supporting:

* Key events
* Listening to log files, waiting for regexp
* Screen shots


## Usage

```shell
$ java -jar defold-robot.jar -s [script] -o [output-dir]
```

### Script

A simple script in JSON:

```json
{
	"logs": {
		"editor-log": {
			"pattern": "editor2.*.log",
			"dir" : "."
		},
		"engine-log": {
			"pattern": "log.txt",
			"dir" : "."
		}
	},
	"steps": [
		["await-log" "editor-log" 5000 "Project loaded"],
		["press" "Shortcut+Shift+R"],
		["type" ".coll"],
		["press" "Down"],
		["press" "Enter"],
		["wait" 1000],
		["screen-capture" "collection"],
		["press" "Shortcut+Shift+R"],
		["type" ".script"],
		["press" "Down"],
		["press" "Enter"],
		["wait" 1000],
		["screen-capture" "script"],
		["press" "Shortcut+B"],
		["wait" 10000],
		["screen-capture" "build"],
		["switch-focus" 1],
		["type" "print('Now reloading')"],
		["press" "Enter"],
		["press" "Shortcut+R"],
		["await-log" "engine-log" 5000 "Now reloading"],
		["press" "Shortcut+Q"],
	]
}
```

In brief, this script:

1. Waits for the editor to say "Project loaded"
2. Open a collection
3. Open a script
4. Run the game
5. Hot-reload the opened script
6. Wait for the engine to confirm it

## Dev

### Testing

Basically, the only complex part is listening for logging.
There are a few rudimentary tests for this.

```shell
lein test
```

### Release a new version

1. Edit the source
2. [optional] Change the version in project.clj
3. Release:

```shell
$ lein release
```
