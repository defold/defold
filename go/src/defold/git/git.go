/*
	Git server library with support for the "smart protocol"

		References:
		http-backend.c in git
		https://github.com/schacon/grack/blob/master/lib/grack.rb

	Note: It's currently not required that http.receivepack is set to true.

*/
package git

import (
	"bytes"
	"compress/zlib"
	"encoding/base64"
	"fmt"
	"io"
	"io/ioutil"
	"log"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"strings"
)

var (
	defaultLog = log.New(os.Stderr, "", log.LstdFlags)
)

/*
	Git server abstraction. Use NewServer() to create an instance.
	The git server implements net.http.Handler.
*/
type Server struct {
	root       string
	gitRoot    string
	accessLog  *log.Logger
	errorLog   *log.Logger
	authorizor Authorizor
}

func init() {
	// Decompress and write embedded git executable

	compressed, _ := base64.StdEncoding.DecodeString(gitCompressedBase64)

	zr, err := zlib.NewReader(bytes.NewBuffer(compressed))
	if err != nil {
		panic(err)
	}
	git, err := ioutil.ReadAll(zr)
	if err != nil {
		panic(err)
	}
	g, err := os.Create("go-git")
	if err != nil {
		panic(err)
	}
	g.Chmod(0777)

	_, err = g.Write(git)
	if err != nil {
		panic(err)
	}
	err = g.Close()
	if err != nil {
		panic(err)
	}
}

/*
	Creates a new git server. Git repositories are located under root. gitRoot is the installation
	directory to git, e.g. /usr.
*/
func NewServer(root, gitRoot string, authorizor Authorizor, accessLog, errorLog *log.Logger) *Server {
	if accessLog == nil {
		accessLog = defaultLog
	}

	if errorLog == nil {
		errorLog = defaultLog
	}

	return &Server{
		root:       root,
		gitRoot:    gitRoot,
		accessLog:  accessLog,
		errorLog:   errorLog,
		authorizor: authorizor,
	}
}

/*
Interface for pluggable authorization
*/
type Authorizor interface {
	// Authorize user for repository. For successful authorization
	// nil is returned, otherwise an error message.
	Authorize(header http.Header, repo string) error
}

type session struct {
	gitRoot  string
	repo     string
	errorLog *log.Logger
}

type gitHandler func(*session, http.ResponseWriter, *http.Request)

type service struct {
	method  string
	handler gitHandler
	pattern *regexp.Regexp
}

var services = []service{
	{"POST", (*session).rpcUploadPack, regexp.MustCompile("(.*?)/git-upload-pack$")},
	{"POST", (*session).rpcReceivePack, regexp.MustCompile("(.*?)/git-receive-pack$")},
	{"GET", (*session).getInfoRefs, regexp.MustCompile("(.*?)/info/refs$")},
}

func writePacket(w io.Writer, s string) error {
	_, e := io.WriteString(w, fmt.Sprintf("%04x%s", len(s)+4, s))
	return e
}

func noCacheHeaders(rw http.ResponseWriter) {
	rw.Header().Set("Expires", "Fri, 01 Jan 1980 00:00:00 GMT")
	rw.Header().Set("Pragma", "no-cache")
	rw.Header().Set("Cache-Control", "no-cache, max-age=0, must-revalidate")
}

// Used to redirect stderr from git to log
type logWriter struct {
	logger *log.Logger
	buf    *bytes.Buffer
}

func (l *logWriter) Write(p []byte) (int, error) {
	if l.buf.Len() < 1024 {
		// Limit error output messages
		l.buf.Write(p)
	}
	return len(p), nil
}

func (l *logWriter) Close() error {
	if l.buf.Len() > 0 {
		l.logger.Println(strings.TrimSpace(l.buf.String()))
	}
	return nil
}

func (s *session) send(w io.Writer, in io.ReadCloser, gitCmd string, args ...string) error {
	path := "./go-git"
	// git-x-y -> x-y
	gitSubCmd := strings.SplitN(gitCmd, "-", 2)[1]
	lw := &logWriter{s.errorLog, bytes.NewBuffer(nil)}
	cmd := &exec.Cmd{
		Path:   path,
		Args:   append([]string{path, gitSubCmd}, args...),
		Stderr: lw,
	}
	cmd.Stdin = in
	cmd.Stdout = w

	defer in.Close()
	defer lw.Close()

	err := cmd.Start()
	if err != nil {
		return err
	}

	if err = cmd.Wait(); err != nil {
		return err
	}
	return nil
}

func (s *session) serviceRpc(serviceName string, rw http.ResponseWriter, r *http.Request) {
	rw.Header().Set("Content-Type", fmt.Sprintf("application/x-git-%s-result", serviceName))
	gitCmd := fmt.Sprintf("git-%s", serviceName)
	if err := s.send(rw, r.Body, gitCmd, "--stateless-rpc", s.repo); err != nil {
		http.Error(rw, "Internal server error", http.StatusInternalServerError)
		s.errorLog.Printf("%s failed: %v", gitCmd, err)
	}
}

func (s *session) rpcUploadPack(rw http.ResponseWriter, r *http.Request) {
	s.serviceRpc("upload-pack", rw, r)
}

func (s *session) rpcReceivePack(rw http.ResponseWriter, r *http.Request) {
	s.serviceRpc("receive-pack", rw, r)
}

func (s *session) getInfoRefs(rw http.ResponseWriter, r *http.Request) {
	service := r.URL.Query().Get("service")
	if !(service == "git-upload-pack" || service == "git-receive-pack") {
		rw.WriteHeader(http.StatusBadRequest)
		io.WriteString(rw, fmt.Sprintf("Invalid service: '%s'\n"))
		return
	}

	serviceName := service[4:]

	// Write info-refs to a buffer first in order to be able
	// to set http error code if an error occur.
	bw := bytes.NewBuffer(nil)

	if err := s.send(bw, r.Body, service, "--stateless-rpc", "--advertise-refs", s.repo); err != nil {
		http.Error(rw, "Internal server error", http.StatusInternalServerError)
		s.errorLog.Printf("%s failed: %v", service, err)
	} else {
		rw.Header().Set("Content-Type", fmt.Sprintf("application/x-git-%s-advertisement", serviceName))
		writePacket(rw, fmt.Sprintf("# service=git-%s\n", serviceName))
		io.WriteString(rw, "0000")
		rw.Write(bw.Bytes())
	}
}

func (s *Server) serveHTTP(rw http.ResponseWriter, r *http.Request) {
	// Currently caching is disabled for all requests
	// Better be safe than sorry
	noCacheHeaders(rw)

	// TODO: We get broken pipe when pushing large data if connection isn't closed
	// Potential bug: https://code.google.com/p/go/issues/detail?id=5660
	rw.Header().Set("Connection", "close")

	for _, srv := range services {
		res := srv.pattern.FindStringSubmatch(r.URL.Path)
		if r.Method == srv.method && len(res) > 0 {

			if err := s.authorizor.Authorize(r.Header, res[1]); err != nil {
				if authHeader := r.Header.Get("Authorization"); authHeader == "" {
					// TODO: Move logic to Authorizor?
					rw.Header().Add("WWW-Authenticate", `Basic realm="HTTP GIT authentication"`)
					http.Error(rw, "Not authorized", http.StatusUnauthorized)
				} else {
					s.errorLog.Printf("Authorization failed: %v", err)
					http.Error(rw, "Forbidden", http.StatusForbidden)
				}
				return
			}

			sess := &session{
				gitRoot:  s.gitRoot,
				repo:     filepath.Join(s.root, res[1]),
				errorLog: s.errorLog}
			srv.handler(sess, rw, r)
			return
		}
	}
	http.NotFound(rw, r)
}

/*
	Indirection to capture http status code
*/
type responseLogger struct {
	rw   http.ResponseWriter
	code int
}

func (l *responseLogger) Header() http.Header {
	return l.rw.Header()
}

func (l *responseLogger) Write(p []byte) (int, error) {
	return l.rw.Write(p)
}

func (l *responseLogger) WriteHeader(code int) {
	l.code = code
	l.rw.WriteHeader(code)
}

/*
	net.http.Handler implementation
*/
func (s *Server) ServeHTTP(rw http.ResponseWriter, r *http.Request) {
	lrw := &responseLogger{rw, 200}
	s.serveHTTP(lrw, r)
	s.accessLog.Printf(`%s "%s %s" %d`, r.RemoteAddr, r.Method, r.RequestURI, lrw.code)
}
