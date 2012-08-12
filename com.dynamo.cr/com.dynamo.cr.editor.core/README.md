Editor Core Plugin
==================

Notes
-----

    Eclipse-RegisterBuddy: slf4j

is added to _MANIFEST.MF_ as _slf4j_ and _logback_ resides in _slf4j_. The class-loader in _slf4j_
must be able to access resources (logback.xml) and classes in this plugin as we define a custom appender in this plugin.
