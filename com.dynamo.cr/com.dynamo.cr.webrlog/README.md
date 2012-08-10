Remote Logging
==============

The web interface is accessible from: http://log.defold.com (or http://defoldlog.appspot.com)

Storage
-------

Data is stored using the following entities:

* __month__: parent entity for both records and issues
* __record__ a single log-record
* __issue__: grouped equivalent log-records

Both records and issues are group by month. Grouping of records into issues is performed in a batch processing step.
Only issues are presented in the web-interface.


REST interface
--------------

__/post__ (POST)

Log record creation

__/issues__ (GET)

Get all active (non-fixed) issues

__/issues/DATE/SHA1/(true|false)__ (PUT)

Mark issue as fixed/not fixed


__/tasks/process__ (GET)

Start batch processing grouping task of records to issues. Should probably a POST request
