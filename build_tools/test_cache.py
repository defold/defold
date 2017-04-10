import os, hashlib, httplib, urllib, json, tempfile, subprocess

def md5(fname):
    hash_md5 = hashlib.md5()
    with open(fname, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            hash_md5.update(chunk)
    return hash_md5.hexdigest()

def strip(fname):
    temp_dir = tempfile.gettempdir()
    temp_path = os.path.join(temp_dir, os.path.basename(fname))
    try:
        p = subprocess.Popen(['strip', '-o', temp_path, fname])
        p.wait()
        if p.returncode == 0:
            return temp_path
        else:
            return fname
    except OSError:
        return fname

timeout = 150
db_server = "metrics.defold.com"
db_port = 8086
db_name = 'enginetests2'

#curl -i -POST "http://metrics.defold.com:8086/write?db=enginetests1" --data-binary 'test_dlib,mytag=checksum value=true'
def lookup(path):
    measurement = path.replace('/', '_')
    stripped = strip(path)
    hash = md5(stripped)
    conn = httplib.HTTPConnection(db_server, db_port, timeout = timeout)
    query = urllib.quote_plus("SELECT * from %s where hash='%s';" % (measurement, hash))
    url = ("/query?db=%s&q=%s" % (db_name, query))
    try:
        conn.request("GET", url)
        response = conn.getresponse()
        if response.status == 200:
            data = json.loads(response.read())
            results = data.get(u'results')
            if not results or len(results) == 0:
                print('test caching inactive: no result')
                return False
            series = results[0].get(u'series')
            if not series or len(series) == 0:
                print('test caching inactive: no series')
                return False
            values = series[0].get(u'values')
            if not values or len(values) == 0:
                print('test caching inactive: no values')
                return False
            return True
        return False
    except IOError:
        return False

def cache(path):
    measurement = path.replace('/', '_')
    stripped = strip(path)
    hash = md5(stripped)
    headers = {"Content-type": "application/x-www-form-urlencoded",
               "Accept": "text/plain"}
    conn = httplib.HTTPConnection(db_server, db_port, timeout = timeout)
    try:
        conn.request("POST", "/write?db=%s" % db_name, "%s,hash=%s value=true" % (measurement, hash), headers)
        response = conn.getresponse()
    except IOError:
        pass
