package main

import (
	"fmt"
	"github.com/msbranco/goconfig"
	"log"
	"net/http"
	"net/http/httputil"
	"net/url"
	"os"
	"strings"
)

type route struct {
	from, to *url.URL
}

type proxy struct {
	host    string
	port    int
	routes  []route
	reverse *httputil.ReverseProxy
}

func newProxy(config string) (*proxy, error) {
	c, err := goconfig.ReadConfigFile(config)
	if err != nil {
		return nil, err
	}

	host := c.GetStringDefault("rproxy", "host", "localhost")
	port := c.GetInt64Default("rproxy", "port", 8000)
	rp := &proxy{host: host, port: int(port), reverse: nil}

	routeStrings, _ := c.GetOptions("rproxy")
	for _, key := range routeStrings {
		if strings.HasPrefix(key, "route_") {
			val := c.GetStringDefault("rproxy", key, "")
			a := strings.SplitN(val, ",", 2)
			if len(a) != 2 {
				return nil, fmt.Errorf("invalid route %v", val)
			}

			trim := strings.TrimSpace

			from, err := url.Parse(trim(a[0]))
			if err != nil {
				return nil, fmt.Errorf("invalid route %v (%v)", val, err)
			}
			if from.Host == "" {
				from.Host = host
			}
			if from.Scheme == "" {
				from.Scheme = "http"
			}
			to, err := url.Parse(trim(a[1]))
			if err != nil {
				return nil, fmt.Errorf("invalid route %v (%v)", val, err)
			}
			rp.routes = append(rp.routes, route{from, to})
		}
	}

	rp.reverse = &httputil.ReverseProxy{Director: rp.director}

	return rp, nil
}

func singleJoiningSlash(a, b string) string {
	aslash := strings.HasSuffix(a, "/")
	bslash := strings.HasPrefix(b, "/")
	switch {
	case aslash && bslash:
		return a + b[1:]
	case !aslash && !bslash && a != "" && b != "":
		return a + "/" + b
	}
	return a + b
}

func match(from, ur url.URL) bool {
	if from.Scheme == ur.Scheme &&
		from.Host == ur.Host {
		return strings.HasPrefix(ur.Path, from.Path)
	}
	return false
}

func (p *proxy) route(ur url.URL) *url.URL {

	best := -1
	n := 0
	for i, r := range p.routes {
		if r.from.Scheme == ur.Scheme &&
			r.from.Host == ur.Host {
			if !match(*r.from, ur) {
				continue
			}
			if n == 0 || len(r.from.Path) > n {
				n = len(r.from.Path)
				best = i
			}
		}
	}

	if best != -1 {
		path := singleJoiningSlash(p.routes[best].to.Path, ur.Path[len(p.routes[best].from.Path):])
		ret := *p.routes[best].to
		ret.Path = path
		ret.RawQuery = ur.RawQuery
		return &ret
	}

	return nil
}

func (p *proxy) ServeHTTP(rw http.ResponseWriter, r *http.Request) {
	r.URL.Scheme = "http"
	r.URL.Host = r.Host
	url := p.route(*r.URL)
	// short circuit if now route is found, otherwise infinite recursion
	if url == nil {
		http.Error(rw, "not found", http.StatusNotFound)
	} else {
		p.reverse.ServeHTTP(rw, r)
	}
}

func (p *proxy) director(req *http.Request) {
	url := p.route(*req.URL)
	*req.URL = *url
	// Set host such that "Host:" is correctly set, i.e. destination host and not proxy host
	req.Host = url.Host
}

func main() {
	if len(os.Args) < 2 {
		fmt.Println("usage: rproxy config")
		os.Exit(5)
	}

	p, err := newProxy(os.Args[1])
	if err != nil {
		log.Fatal(err)
	}

	s := http.Server{
		Addr:    fmt.Sprintf("%s:%d", p.host, p.port),
		Handler: p,
	}
	err = s.ListenAndServe()
	if err != nil {
		log.Fatal(err)
	}
}
