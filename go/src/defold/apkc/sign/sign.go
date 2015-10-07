/*
Jar-signing functionality (apk files).

The library is designed for certificates
in PEM-format and key in pk8-format. It's possible to extract certificate and key
from java keystore with keytool and openssl.

	Create certifciate and key with openssl:
	openssl genrsa -out key.pem 1024
	openssl req -new -key key.pem -out request.pem
	openssl x509 -req -days 9999 -in request.pem -signkey key.pem -out certificate.pem
	openssl pkcs8 -topk8 -outform DER -in key.pem -inform PEM -out key.pk8 -nocrypt

	Convert certificate for facebook:
	openssl x509 -in certificate.pem -inform PEM -out certificate.der -outform DER
	cat certificate.der | openssl sha1 -binary | openssl base64


	Various usefull links read when creating this lib:
	http://qistoph.blogspot.se/2012/01/manual-verify-pkcs7-signed-data-with.html
	http://www.docjar.com/html/api/org/apache/harmony/security/pkcs7/SignedData.java.html
	http://www.umich.edu/~x509/ssleay/asn1-oids.html
	http://www.research.ibm.com/trl/projects/xml/xss4j/data/asn1/grammars/pkcs7.asn
	https://github.com/keesj/gomo/wiki/AndroidPackageSignatures
	https://github.com/android/platform_build/blob/master/tools/signapk/SignApk.java

	Dump ASN.1 tree:
	openssl asn1parse -i -inform der -in FILE
*/
package sign

import (
	"archive/zip"
	"bytes"
	"crypto"
	"crypto/rand"
	"crypto/rsa"
	"crypto/sha1"
	"crypto/x509"
	"crypto/x509/pkix"
	"encoding/asn1"
	"encoding/base64"
	"encoding/pem"
	"fmt"
	"io"
	"io/ioutil"
	"math/big"
	"os"
	"time"
)

func digest(fn string) (string, error) {
	file, e := os.Open(fn)
	if e != nil {
		return "", e
	}
	defer file.Close()
	s := sha1.New()
	io.Copy(s, file)

	return base64.StdEncoding.EncodeToString(s.Sum(nil)), nil
}

func signature(name, digest string) string {
	str := fmt.Sprintf("Name: %s\r\nSHA1-Digest: %s\r\n\r\n", name, digest)
	s := sha1.New()
	io.WriteString(s, str)
	return base64.StdEncoding.EncodeToString(s.Sum(nil))
}

var (
	// asn.1 object identifiers
	pkcs7DataOID       = asn1.ObjectIdentifier{1, 2, 840, 113549, 1, 7, 1}
	pkcs7SignedDataOID = asn1.ObjectIdentifier{1, 2, 840, 113549, 1, 7, 2}
	sha1OID            = asn1.ObjectIdentifier{1, 3, 14, 3, 2, 26}
	rsaEncryption      = asn1.ObjectIdentifier{1, 2, 840, 113549, 1, 1, 1}
)

var (
	nullObject = asn1.RawValue{Tag: 5}
)

/*
pkcs7 data-structures. The data-structures aren't strictly modified
after the spec.
*/
type contentInfo struct {
	ContentType asn1.ObjectIdentifier
	Content     asn1.RawValue `asn1:"optional"`
}

type digestAlgorithmIdentifiers struct {
	Algorithms asn1.ObjectIdentifier
	Parameters asn1.RawValue `asn1:"optional"`
}

type signedData struct {
	Version          int
	DigestAlgorithms []digestAlgorithmIdentifiers `asn1:"set"`
	ContentInfo      contentInfo
	Certificates     asn1.RawValue
	SignerInfos      []signerInfo `asn1:"set"`
}

type issuerAndSerialNumber struct {
	Issuer       asn1.RawValue
	SerialNumber *big.Int
}

type signerInfo struct {
	Version                   int
	SignerIdentifier          issuerAndSerialNumber
	DigestAlgorithm           digestAlgorithmIdentifiers
	DigestEncryptionAlgorithm digestAlgorithmIdentifiers
	Signature                 []byte
}

type Signer struct {
	certificate *x509.Certificate
	key         *rsa.PrivateKey
}

func loadPemCert(fileName string) (*x509.Certificate, error) {
	data, err := ioutil.ReadFile(fileName)
	if err != nil {
		return nil, err
	}

	pemBlock, _ := pem.Decode(data)
	if pemBlock == nil {
		return nil, fmt.Errorf("unable to parse certificate %s", fileName)
	}

	certList, err := x509.ParseCertificates(pemBlock.Bytes)
	if err != nil {
		return nil, err
	}

	if len(certList) == 0 {
		return nil, fmt.Errorf("no certificates found")
	}

	return certList[0], nil
}

func loadKey(keyName string) (*rsa.PrivateKey, error) {
	data, err := ioutil.ReadFile(keyName)
	if err != nil {
		return nil, err
	}

	key, err := x509.ParsePKCS8PrivateKey(data)
	if err != nil {
		return nil, err
	}

	if rsaKey, ok := key.(*rsa.PrivateKey); ok {
		return rsaKey, nil
	} else {
		return nil, fmt.Errorf("key is not a rsa-key")
	}
}

/*
Create a new signer with certificate and key from file.
Certificate in pem-formant and key in pk8-format
*/
func NewSigner(certFile, keyFile string) (*Signer, error) {
	cert, err := loadPemCert(certFile)
	if err != nil {
		return nil, err
	}

	key, err := loadKey(keyFile)
	if err != nil {
		return nil, err
	}

	return &Signer{cert, key}, nil
}

/*
Create a new debug-signer with automatically generated key-pair
*/
func NewDebugSigner() (*Signer, error) {
	priv, err := rsa.GenerateKey(rand.Reader, 1024)
	if err != nil {
		return nil, err
	}

	notBefore := time.Now()
	notAfter := time.Date(2049, 12, 31, 23, 59, 59, 0, time.UTC)

	template := x509.Certificate{
		SerialNumber: new(big.Int).SetInt64(0),
		Subject: pkix.Name{
			Organization: []string{"Acme Co"},
		},
		NotBefore: notBefore,
		NotAfter:  notAfter,

		KeyUsage:              x509.KeyUsageKeyEncipherment | x509.KeyUsageDigitalSignature,
		ExtKeyUsage:           []x509.ExtKeyUsage{x509.ExtKeyUsageCodeSigning},
		BasicConstraintsValid: true,
	}

	derBytes, err := x509.CreateCertificate(rand.Reader, &template, &template, &priv.PublicKey, priv)
	if err != nil {
		return nil, err
	}

	certList, err := x509.ParseCertificates(derBytes)
	if err != nil {
		return nil, err
	}

	return &Signer{certList[0], priv}, nil
}

func (s *Signer) sign(in io.Reader) ([]byte, error) {
	data, err := ioutil.ReadAll(in)
	if err != nil {
		return nil, err
	}
	sha1 := crypto.SHA1.New()
	sha1.Write(data)

	return rsa.SignPKCS1v15(rand.Reader, s.key, crypto.SHA1, sha1.Sum(nil))
}

func (s *Signer) createSignatureBlock(in io.Reader) ([]byte, error) {
	sig, err := s.sign(in)
	if err != nil {
		return nil, err
	}

	si := signerInfo{1,
		issuerAndSerialNumber{asn1.RawValue{FullBytes: s.certificate.RawIssuer}, s.certificate.SerialNumber},
		digestAlgorithmIdentifiers{sha1OID, nullObject},
		digestAlgorithmIdentifiers{rsaEncryption, nullObject},
		sig,
	}

	pkcs := contentInfo{pkcs7SignedDataOID, asn1.RawValue{}}
	sd := signedData{1,
		[]digestAlgorithmIdentifiers{digestAlgorithmIdentifiers{sha1OID, nullObject}},
		contentInfo{pkcs7DataOID, asn1.RawValue{}},
		asn1.RawValue{Bytes: s.certificate.Raw, IsCompound: true, Class: 2},
		[]signerInfo{si},
	}

	sdAsn1, err := asn1.Marshal(sd)
	if err != nil {
		return nil, err
	}

	pkcs.Content.Class = 2
	pkcs.Content.IsCompound = true
	pkcs.Content.Bytes = sdAsn1

	pkcsAsn1, err := asn1.Marshal(pkcs)
	if err != nil {
		return nil, err
	}

	return pkcsAsn1, nil
}

type entry struct {
	name      string
	digest    string
	signature string
}

func (s *Signer) createManifestFile(entries []entry) []byte {
	w := bytes.NewBuffer(nil)

	w.WriteString("Manifest-Version: 1.0\r\n")
	w.WriteString("Created-By: 1.0 (Android)\r\n")
	w.WriteString("\r\n")

	for _, e := range entries {
		w.WriteString(fmt.Sprintf("Name: %s\r\n", e.name))
		w.WriteString(fmt.Sprintf("SHA1-Digest: %s\r\n", e.digest))
		w.WriteString("\r\n")
	}

	return w.Bytes()
}

func (s *Signer) createSignatureFile(manifest []byte, entries []entry) []byte {
	sh := sha1.New()
	sh.Write(manifest)

	w := bytes.NewBuffer(nil)

	w.WriteString("Signature-Version: 1.0\r\n")
	w.WriteString("Created-By: 1.0 (Android)\r\n")
	w.WriteString(fmt.Sprintf("SHA1-Digest-Manifest: %s\r\n", base64.StdEncoding.EncodeToString(sh.Sum(nil))))
	w.WriteString("\r\n")

	for _, e := range entries {
		w.WriteString(fmt.Sprintf("Name: %s\r\n", e.name))
		w.WriteString(fmt.Sprintf("SHA1-Digest: %s\r\n", e.signature))
		w.WriteString("\r\n")
	}

	return w.Bytes()
}

/*
Sign zip (or apk/jar)
*/
func (s *Signer) SignZip(inFile, outFile string) error {
	r, err := zip.OpenReader(inFile)
	if err != nil {
		return err
	}
	defer r.Close()

	of, err := os.Create(outFile)
	if err != nil {
		return err
	}
	defer of.Close()
	w := zip.NewWriter(of)
	defer w.Close()

	entries := []entry{}

	skipFiles := map[string]bool{
		"META-INF/MANIFEST.MF": true,
		"META-INF/CERT.SF":     true,
		"META-INF/CERT.RSA":    true,
	}

	now := time.Now()

	for _, f := range r.File {
		rc, _ := f.Open()
		data, _ := ioutil.ReadAll(rc)
		rc.Close()

		s := sha1.New()
		s.Write(data)
		d := base64.StdEncoding.EncodeToString(s.Sum(nil))
		e := entry{
			f.Name,
			d,
			signature(f.Name, d),
		}

		if !skipFiles[f.Name] {
			h := zip.FileHeader{
				Name: f.Name,
				Method: f.Method,
			}
			h.SetModTime(now)
			ze, _ := w.CreateHeader(&h)
			ze.Write(data)
		}

		entries = append(entries, e)
	}

	manifest := s.createManifestFile(entries)
	signature := s.createSignatureFile(manifest, entries)
	signatureBlock, err := s.createSignatureBlock(bytes.NewReader(signature))
	if err != nil {
		return err
	}

	ze, _ := w.Create("META-INF/MANIFEST.MF")
	ze.Write(manifest)

	ze, _ = w.Create("META-INF/CERT.SF")
	ze.Write(signature)

	ze, _ = w.Create("META-INF/CERT.RSA")
	ze.Write(signatureBlock)

	return nil
}
