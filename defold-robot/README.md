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

A simple script in EDN:

```edn
{:logs {:editor {:pattern "editor2.*.log"
                 :dir "."}
        :engine {:pattern "log.txt"
                 :dir "."}}
 :steps [[:await-log :editor 60000 "project loaded"]
         [:wait 2000]
         [:screen-capture "project_loaded"]
         [:press :command :shift :r]
         [:type ".coll"]
         [:press :down]
         [:press :enter]
         [:wait 2500]
         [:screen-capture "collection"]
         [:press :command :shift :r]
         [:type "player.script"]
         [:press :down]
         [:press :enter]
         [:wait 2000]
         [:screen-capture "script"]
         [:press :command :b]
         [:await-log :engine 10000 "intro started"]
         [:await-log :engine 60000 " seconds"]
         [:screen-capture "game_intro"]
         [:await-log :engine 60000 "level started"]
         [:await-log :engine 60000 " seconds"]
         [:screen-capture "game_running"]
         [:switch-focus]
         [:press :enter]
         [:press :command :r]
         [:press :command :z]
         [:await-log :engine 3000 "successfully reloaded"]
         [:screen-capture "test_end"]
         [:switch-focus]
         [:press :command :q]
         [:press :command :q]]}
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
