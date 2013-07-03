package sign

import (
	"archive/zip"
	"bytes"
	"io/ioutil"
	"os"
	"testing"
)

func TestDigest(t *testing.T) {
	exp := "cV/curTIemB5Hi4FPO8KXds3N6Y="
	d, err := digest("test/AndroidManifest.xml")
	if err != nil {
		t.Fatal(err)
	}
	if d != exp {
		t.Errorf("expected digest %s, got %s", exp, d)
	}
}

func TestSignature(t *testing.T) {
	exp := "9MuXFB3mvN1pKUf6nxeB+zwF1dc="
	sig := signature("AndroidManifest.xml", "cV/curTIemB5Hi4FPO8KXds3N6Y=")

	if sig != exp {
		t.Errorf("expected signature %s, got %s", exp, sig)
	}
}

func TestPKCS7SignatureBlock(t *testing.T) {
	s, err := NewSigner("test/certificate.pem", "test/key.pk8")
	if err != nil {
		t.Fatal(err)
	}

	in, err := os.Open("test/CERT.SF")
	if err != nil {
		t.Fatal(err)
	}
	defer in.Close()

	sig, err := s.createSignatureBlock(in)
	if err != nil {
		t.Fatal(err)
	}

	ref, err := ioutil.ReadFile("test/CERT.RSA")
	if err != nil {
		t.Fatal(err)
	}

	if bytes.Compare(sig, ref) != 0 {
		t.Errorf("expected %v, got %v", ref, sig)
	}
}

func TestDebugSign(t *testing.T) {
	s, err := NewDebugSigner()
	if err != nil {
		t.Fatal(err)
	}

	in, err := os.Open("test/CERT.SF")
	if err != nil {
		t.Fatal(err)
	}
	defer in.Close()

	// We have nothing to compare against here
	// as the debug-signer created a new key-pair
	// for every invocation
	_, err = s.createSignatureBlock(in)
	if err != nil {
		t.Fatal(err)
	}
}

func TestSignZip(t *testing.T) {
	// This is only a smoke-test
	// Manual verification can be performed by running
	// jarsigner -verify tmp/test_signed.zip

	os.Mkdir("tmp", 0777)
	zipFile, err := os.Create("tmp/test.zip")
	if err != nil {
		t.Fatal(err)
	}

	testData := []byte("test data")

	zw := zip.NewWriter(zipFile)
	e, err := zw.Create("test")
	if err != nil {
		t.Fatal(err)
	}

	_, err = e.Write(testData)
	if err != nil {
		t.Fatal(err)
	}

	zw.Close()

	s, err := NewSigner("test/certificate.pem", "test/key.pk8")
	if err != nil {
		t.Fatal(err)
	}

	err = s.SignZip("tmp/test.zip", "tmp/test_signed.zip")
	if err != nil {
		t.Fatal(err)
	}

}
