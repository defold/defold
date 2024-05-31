# Bob the builder

## Content pipeline

The primary build tool is bob. Bob is used for the editor but also for engine-tests. In the first build-step a standalone version of bob is built. A legacy pipeline, waf/python and some classes from bob.jar, is still used for gamesys and for built-in content. This might be changed in the future but integrating bob with waf 1.5.x is pretty hard as waf 1.5.x is very restrictive where source and built content is located. Built-in content is compiled, via .arc-files, to header-files, installed to $DYNAMO_HOME, etc In other words tightly integrated with waf.


### Byte order/endian

By convention all graphics resources are explicitly in little-endian and specifically ByteOrder.LITTLE_ENDIAN in Java. Currently we support only little endian architectures. If this is about to change we would have to byte-swap at run-time or similar. As run-time editor code and pipeline code often is shared little-endian applies to both. For specific editor-code ByteOrder.nativeOrder() is the correct order to use.


### Updating "Build Report" template

The build report template is a single HTML file found under `com.dynamo.cr/com.dynamo.cr.bob/lib/report_template.html`. Third party JS and CSS libraries used (DataTables.js, Jquery, Bootstrap, D3 and Dimple.js) are concatenated into two HTML inline tags and added to this file. If the libraries need to be updated/changed please use the `inline_libraries.py` script found in `share/report_libs/`.

