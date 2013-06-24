package git

import (
	"crypto/md5"
	"encoding/base64"
	"fmt"
	"io/ioutil"
	"math/rand"
	"net"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"testing"
)

var (
	smallFileContent = "some data"
	largeFileContent = []byte{}
	largeFileSize    = 1024 * 1024 * 2
)

func init() {
	largeFileContent = make([]byte, largeFileSize)
	for i := 0; i < largeFileSize; i++ {
		largeFileContent[i] = byte(rand.Int() & 0xff)
	}
}

type AccessAll struct {
}

func (a *AccessAll) Authorize(header http.Header, repo string) error {
	return nil
}

type ForbidAll struct {
}

func (a *ForbidAll) Authorize(header http.Header, repo string) error {
	return fmt.Errorf("Permission denied")
}

type HttpAuth struct {
}

func parseBasicAuth(authHeader string) (string, string, error) {
	auth, _ := base64.StdEncoding.DecodeString(authHeader[6:])
	userPass := strings.Split(string(auth), ":")
	if len(userPass) == 2 {
		return userPass[0], userPass[1], nil
	}
	return "", "", fmt.Errorf("malformed auth header")
}

func (a *HttpAuth) Authorize(header http.Header, repo string) error {

	authHeader := header.Get("Authorization")
	if authHeader != "" {
		if user, pass, err := parseBasicAuth(authHeader); err == nil {
			if user == "foo" && pass == "bar" {
				return nil
			}
		} else {
			return err
		}
	}

	return fmt.Errorf("Permission denied")
}

func newTemplate() string {
	d, _ := ioutil.TempDir(os.TempDir(), "gitsrv")
	gd := filepath.Join(d, ".git")
	if e := exec.Command("git", "init", d).Run(); e != nil {
		panic(e)
	}

	small := filepath.Join(d, "small")
	ioutil.WriteFile(small, []byte(smallFileContent), 0777)

	large := filepath.Join(d, "large")
	ioutil.WriteFile(large, largeFileContent, 0777)

	if out, e := exec.Command("git", "--work-tree="+d, "--git-dir="+gd, "add", ".").CombinedOutput(); e != nil {
		panic(string(out))
	}
	if out, e := exec.Command("git", "--work-tree="+d, "--git-dir="+gd, "commit", "-a", "-minitial").CombinedOutput(); e != nil {
		panic(string(out))
	}

	return d
}

func cloneRepoBare(repo string) string {
	d, _ := ioutil.TempDir(os.TempDir(), "gitsrv")
	if out, e := exec.Command("git", "clone", "--bare", repo, d).CombinedOutput(); e != nil {
		panic(string(out))
	}

	if out, e := exec.Command("git", "--git-dir="+d, "config", "http.receivepack", "true").CombinedOutput(); e != nil {
		panic(string(out))
	}

	return d
}

type gitTest struct {
	template   string
	repo       string
	listener   net.Listener
	port       int
	httpServer *http.Server
}

func setup(auth Authorizor) *gitTest {
	template := newTemplate()
	repo := cloneRepoBare(template)

	s := NewServer(repo, auth, nil, nil)

	l, e := net.Listen("tcp", ":0")
	if e != nil {
		panic(e)
	}

	port := l.Addr().(*net.TCPAddr).Port

	httpServer := &http.Server{Addr: fmt.Sprintf("%d", port), Handler: s}
	go httpServer.Serve(l)

	return &gitTest{template: template,
		repo:       repo,
		listener:   l,
		port:       port,
		httpServer: httpServer,
	}
}

func shutdown(gt *gitTest) {
	gt.listener.Close()
	os.RemoveAll(gt.template)
	os.RemoveAll(gt.repo)
}

func TestClone(t *testing.T) {
	gt := setup(new(AccessAll))
	defer shutdown(gt)

	clone, _ := ioutil.TempDir(os.TempDir(), "gitsrv")
	if out, e := exec.Command("git", "clone", fmt.Sprintf("http://localhost:%d", gt.port), clone).CombinedOutput(); e != nil {
		panic(string(out))
	}

	defer os.RemoveAll(clone)
	small, err := ioutil.ReadFile(filepath.Join(clone, "small"))
	if err != nil {
		panic(err)
	}

	if string(small) != smallFileContent {
		t.Errorf("expected %v, found %v", smallFileContent, string(small))
	}

	large, err := ioutil.ReadFile(filepath.Join(clone, "large"))
	if err != nil {
		panic(err)
	}

	md5repo := md5.New()
	md5repo.Write(large)
	md5ref := md5.New()
	md5ref.Write(largeFileContent)

	md5repos := fmt.Sprintf("%v", md5repo.Sum(nil))
	md5refs := fmt.Sprintf("%v", md5ref.Sum(nil))

	if md5repos != md5refs {
		t.Errorf("expected md5 for large file %v, found %v", md5refs, md5repos)

	}
}

func TestAuth(t *testing.T) {
	gt := setup(new(ForbidAll))
	defer shutdown(gt)

	clone, _ := ioutil.TempDir(os.TempDir(), "gitsrv")
	defer os.RemoveAll(clone)
	if _, e := exec.Command("git", "clone", fmt.Sprintf("http://foo:bar@localhost:%d", gt.port), clone).CombinedOutput(); e == nil {
		t.Error("expected permission denied error")
	}
}

func TestHttpAuth(t *testing.T) {
	gt := setup(new(HttpAuth))
	defer shutdown(gt)

	clone, _ := ioutil.TempDir(os.TempDir(), "gitsrv")
	defer os.RemoveAll(clone)
	if _, e := exec.Command("git", "clone", fmt.Sprintf("http://foo:bar_@localhost:%d", gt.port), clone).CombinedOutput(); e == nil {
		t.Error("expected permission denied error")
	}

	if out, e := exec.Command("git", "clone", fmt.Sprintf("http://foo:bar@localhost:%d", gt.port), clone).CombinedOutput(); e != nil {
		t.Error(string(out))
	}
}
