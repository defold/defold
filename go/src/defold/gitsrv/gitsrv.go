/*
	Git server in go
*/

// +build !windows

package main

import (
	"bytes"
	"defold/git"
	"encoding/base64"
	"encoding/json"
	"flag"
	"fmt"
	"io/ioutil"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"time"
)

var (
	crUrl        = ""
	port         = 0
	repoRoot     = ""
	logDir       = ""
	readTimeOut  = 0
	writeTimeOut = 0
)

func init() {
	flag.StringVar(&crUrl, "cr-url", "", "cr server url")
	flag.IntVar(&port, "port", 8080, "http port")
	flag.StringVar(&repoRoot, "repo-root", "", "repository root")
	flag.StringVar(&logDir, "log-dir", ".", "log directory")
	flag.IntVar(&readTimeOut, "read-timeout", 100, "http read timeout in seconds")
	flag.IntVar(&writeTimeOut, "write-timeout", 100, "http write timeout in seconds")
	flag.Parse()
}

type HttpCrAuth struct {
	crUrl string
}

type userInfo struct {
	Id        int64  `json:"id"`
	Email     string `json:"email"`
	FirstName string `json:"first_name"`
	LastName  string `json:"last_name"`
}

func (a *HttpCrAuth) getUserID(email, pass string) (int64, error) {
	url := fmt.Sprintf("%s/users/%s", a.crUrl, email)
	req, _ := http.NewRequest("GET", url, nil)
	req.SetBasicAuth(email, pass)
	req.Header.Add("Accept", "application/json")
	client := &http.Client{}
	if resp, err := client.Do(req); err == nil {
		buf, _ := ioutil.ReadAll(resp.Body)
		if resp.StatusCode != http.StatusOK {
			return -1, fmt.Errorf("%s (%s)", resp.Status, string(buf))
		}
		ui := new(userInfo)
		if err := json.NewDecoder(bytes.NewReader(buf)).Decode(ui); err == nil {
			return ui.Id, nil
		} else {
			return -1, err
		}
	} else {
		return -1, err
	}
}

func parseBasicAuth(authHeader string) (string, string, error) {
	auth, _ := base64.StdEncoding.DecodeString(authHeader[6:])
	userPass := strings.Split(string(auth), ":")
	if len(userPass) == 2 {
		return userPass[0], userPass[1], nil
	}
	return "", "", fmt.Errorf("malformed auth header")
}

func (a *HttpCrAuth) Authorize(header http.Header, repo string) error {
	arr := strings.Split(repo, "/")
	if len(arr) < 2 {
		return fmt.Errorf("Invalid repo path")
	}
	// NOTE: Project-id is the last element by convention
	prj := arr[len(arr)-1]

	authHeader := header.Get("Authorization")

	if authHeader != "" {
		email, pass, err := parseBasicAuth(authHeader)
		if err != nil {
			return err
		}

		userId, err := a.getUserID(email, pass)
		if err != nil {
			return err
		}

		url := fmt.Sprintf("%s/projects/%d/%s/project_info", a.crUrl, userId, prj)
		req, _ := http.NewRequest("GET", url, nil)
		client := &http.Client{}
		req.SetBasicAuth(email, pass)

		resp, err := client.Do(req)
		if err != nil {
			return err
		}

		if resp.StatusCode == http.StatusOK {
			return nil
		} else {
			return fmt.Errorf("Permission denied (%v)", resp.Status)
		}
	}

	return fmt.Errorf("Permission denied")
}

func main() {
	if crUrl == "" {
		log.Fatal("--cr-url not specified")
	}
	if repoRoot == "" {
		log.Fatal("--repo-root not specified")
	}

	accessFile, err := os.OpenFile(filepath.Join(logDir, "git-access.log"), os.O_CREATE|os.O_APPEND|os.O_RDWR, 0600)
	if err != nil {
		log.Fatal(err)
	}

	errorFile, err := os.OpenFile(filepath.Join(logDir, "git-error.log"), os.O_CREATE|os.O_APPEND|os.O_RDWR, 0600)
	if err != nil {
		log.Fatal(err)
	}

	accessLog := log.New(accessFile, "", log.LstdFlags)
	errorLog := log.New(errorFile, "", log.LstdFlags)

	s := git.NewServer(repoRoot, &HttpCrAuth{crUrl}, accessLog, errorLog)
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
