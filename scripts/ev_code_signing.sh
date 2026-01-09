#!/usr/bin/env bash
# Copyright 2020-2026 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
#
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.



gcloud_keyfile="defold-editor-db2d7e2b52d7.json"
gcloud_projectid="defold-editor"
gcloud_location="europe-west3"
gcloud_keyringname="ev-key-ring"
gcloud_keyname="ev-windows-key"
gcloud_certfile="codesign-chain.pem"

keystore="projects/${gcloud_projectid}/locations/${gcloud_location}/keyRings/${gcloud_keyringname}"
jsign="tmp/dynamo_home/ext/share/java/jsign-4.2.jar"

gcloud auth activate-service-account --key-file="${gcloud_keyfile}"

java -jar "${jsign}" --storetype GOOGLECLOUD --storepass "$(gcloud auth print-access-token)" --keystore "${keystore}" --alias "${gcloud_keyname}" --certfile "${gcloud_certfile}" --tsmode 'RFC3161' --tsaurl 'http://timestamp.globalsign.com/tsa/r6advanced1' Defold.exe