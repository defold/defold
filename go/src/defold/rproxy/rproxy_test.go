package main

import (
	"net/url"
	"testing"
)

func parse(u string) url.URL {
	ur, err := url.Parse(u)
	if err != nil {
		panic(err)
	}
	return *ur
}

func TestReverseProxy(t *testing.T) {
	rp, err := newProxy("test.config")
	if err != nil {
		t.Fatal(err)
	}

	var tests = []struct {
		url      string
		expected string
	}{
		{"http://localhost", "http://localhost:8000"},
		{"http://localhost/", "http://localhost:8000/"},
		{"http://localhost/foo/bar", "http://localhost:8000/foo/bar"},
		{"http://localhost:9998", "http://localhost:8000"},
		{"http://localhost:9998/", "http://localhost:8000/"},
		{"http://localhost:9998/foo/bar", "http://localhost:8000/foo/bar"},

		{"http://localhost/downloads", "http://localhost:8080/downloads"},
		{"http://localhost/downloads/", "http://localhost:8080/downloads/"},
		{"http://localhost/downloads/file.txt", "http://localhost:8080/downloads/file.txt"},

		{"http://localhost/prjs", "http://localhost:8001"},
		{"http://localhost/prjs/a", "http://localhost:8001/a"},

		{"http://localhost:9998/prjs", "http://localhost:8001"},
		{"http://localhost:9998/prjs/a", "http://localhost:8001/a"},

		{"http://localhost/reddit", "http://www.reddit.com/r"},
		{"http://localhost/reddit/programming", "http://www.reddit.com/r/programming"},

		{"http://localhost/short", "http://localhost:9000/short"},
	}

	for _, tt := range tests {
		u := rp.route(parse(tt.url))
		if u == nil {
			t.Errorf("nil route for %v", tt.url)
		}
		e := parse(tt.expected)
		if *u != e {
			t.Errorf("expected %v, got %v", e, *u)
		}
	}
}
