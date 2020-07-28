// Copyright 2020 The Defold Foundation
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
// 
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
// 
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

/*
	APK tool.

	Currently only signing is supported but adding resouces and
	converting AndroidManifest.xml might be added in the future.

	For the binary xml-format used in Android see base/libs/androidfw/ResourceType.cpp,
	base/include/androidfw/ResourceType.h and base/tools/aapt/XMLNode.cpp (flatten, flatten_node, etc)
	Note that compiling AndroidManfiest.xml seems to require data from android.jar

		Convert AndroidManifest.xml to binary version:
		aapt package -M AndroidManifest.xml -I .../android-sdk/platforms/android-17/android.jar -F tmp.zip
*/
package main

import (
	"defold/apkc/sign"
	"flag"
	"fmt"
	"log"
	"os"
)

var (
	cert   = ""
	key    = ""
	inZip  = ""
	outZip = ""
)

func init() {
	flag.StringVar(&inZip, "in", "", "in file (unsigned)")
	flag.StringVar(&outZip, "out", "", "out file (signed)")
	flag.StringVar(&cert, "cert", "", "certificate (pem format)")
	flag.StringVar(&key, "key", "", "key (pk8 format)")
	flag.Usage = func() {
		fmt.Fprintf(os.Stderr, "usage: apkc [options]\n")
		flag.PrintDefaults()
		fmt.Fprintf(os.Stderr, "\nIf -cert and -key are left out a key-pair is automatically generated.\n")
	}
}

func main() {
	flag.Parse()

	if inZip == "" || outZip == "" {
		flag.Usage()
		os.Exit(5)
	}

	var s *sign.Signer
	var err error

	if cert == "" && key == "" {
		s, err = sign.NewDebugSigner()
	} else {
		s, err = sign.NewSigner(cert, key)
	}

	if err != nil {
		log.Fatal(err)
	}

	err = s.SignZip(inZip, outZip)
	if err != nil {
		log.Fatal(err)
	}
}
