slf4j and logback
=================

Notes
-----

The following

    Eclipse-BuddyPolicy: registered

is added to _MANIFEST.MF_ as _sl4fj_ and _logback_ resides in this plugin. In order to extend _logback_ with custom appenders
classes must be lodaded with the classloader from this plugin.
