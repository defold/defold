Plugin with misc stuff
======================

This plug-in contains a lot of misc stuff shared by editor and server. We should really split this into several.

Notes
-----

Beware of the following:

* The dependences on javax.mail and javax.activation is for the SMTPAppender in logback. If logback is moved to a separate plugin
  don't forget to add these dependencies to the new.

