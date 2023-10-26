#!/usr/bin/env bash

file=${1}

gcloud_projectid="defold-editor"
gcloud_location="europe-west3"
gcloud_keyringname="ev-key-ring"
gcloud_keyname="ev-windows-key"
gcloud_certfile="ci/gcloud_certfile.cer"
gcloud_keyfile="ci/gcloud_keyfile.json"

keystore="projects/${gcloud_projectid}/locations/${gcloud_location}/keyRings/${gcloud_keyringname}"

jsign="${DYNAMO_HOME}/ext/share/java/jsign-4.2.jar"

gcloud auth activate-service-account --key-file="${gcloud_keyfile}"

java -jar "${jsign}" --storetype GOOGLECLOUD --storepass "$(gcloud auth print-access-token)" --keystore "${keystore}" --alias "${gcloud_keyname}" --certfile "${gcloud_certfile}" --tsmode 'RFC3161' --tsaurl 'http://timestamp.globalsign.com/tsa/r6advanced1' ${file}
