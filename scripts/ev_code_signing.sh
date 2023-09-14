#!/usr/bin/env bash

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