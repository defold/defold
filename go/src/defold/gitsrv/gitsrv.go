/*
	Git server in go
*/
package main

import (
	"defold/git"
	"flag"
	"fmt"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"time"
)

var (
	port         = 0
	repoRoot     = ""
	gitRoot      = ""
	logDir       = ""
	readTimeOut  = 0
	writeTimeOut = 0
)

func init() {
	flag.IntVar(&port, "port", 8080, "http port")
	flag.StringVar(&repoRoot, "repo-root", "", "repository root")
	flag.StringVar(&gitRoot, "git-root", "/usr", "git install root directory")
	flag.StringVar(&logDir, "log-dir", ".", "log directory")
	flag.IntVar(&readTimeOut, "read-timeout", 100, "http read timeout in seconds")
	flag.IntVar(&writeTimeOut, "write-timeout", 100, "http write timeout in seconds")
	flag.Parse()
}

// TODO: Temporary solution until proper authorization is implemented.
// See https://defold.fogbugz.com/default.asp?2159
type AccessAll int

func (a *AccessAll) Authorize(header http.Header, repo string) error {
	return nil
}

func main() {
	if fi, _ := os.Stat(filepath.Join(gitRoot, "libexec/git-core")); fi == nil {
		log.Fatalf("%s not found. Invalid git installation?", filepath.Join(gitRoot, "libexec/git-core"))
	}

	accessFile, err := os.OpenFile(filepath.Join(logDir, "access.log"), os.O_CREATE|os.O_APPEND|os.O_RDWR, 0600)
	if err != nil {
		log.Fatal(err)
	}

	errorFile, err := os.OpenFile(filepath.Join(logDir, "error.log"), os.O_CREATE|os.O_APPEND|os.O_RDWR, 0600)
	if err != nil {
		log.Fatal(err)
	}

	accessLog := log.New(accessFile, "", log.LstdFlags)
	errorLog := log.New(errorFile, "", log.LstdFlags)

	s := git.NewServer(repoRoot, gitRoot, new(AccessAll), accessLog, errorLog)
	httpServer := &http.Server{
		Addr:         fmt.Sprintf(":%d", port),
		Handler:      s,
		ReadTimeout:  time.Duration(readTimeOut) * time.Second,
		WriteTimeout: time.Duration(writeTimeOut) * time.Second,
	}

	err = httpServer.ListenAndServe()
	if err != nil {
		log.Fatal(err)
	}
}
