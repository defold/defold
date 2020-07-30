## Simple reverse proxy server

### Configuration

* **host:** proxy host name
* **port:** proxy port
* **route_x:** route x in format from-url,to-url

For every route the longest path match is selected. The from-url is replaced
by the to-url with the non-matching part of the source url appended, i.e. the non-prefix part.

#### Example:

**route:** http://localhost:8000/re, http://www.reddit.com/r

The url http://localhost:8000/re/programming is routed to http://www.reddit.com/r/programming


#### Example configuration

	[rproxy]
	host = localhost
	port = 8000
	route_1 = http://localhost:8000/svd, http://www.svd.se
	route_2 = http://localhost:8000/reddit, http://www.reddit.com/r
	

from-url has a short version - /path. Path is translated to an url on the following
form: http://proxy-host/path. Note that the proxy-port isn't set.
