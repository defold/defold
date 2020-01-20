#!/bin/sh -l

REPO=$1
TOKEN=$2
ACTION=$3
PAYLOAD=$4

if [ -z "${REPO}" ]; then
	echo "You must provide a repository to dispatch event to (:owner/:repo)"
	exit 1
fi
if [ -z "${TOKEN}" ]; then
	echo "You must provide a GitHub authentication token"
	exit 1
fi
if [ -z "${ACTION}" ]; then
	echo "You must provide an action/webhook event name"
	exit 1
fi
if [ -z "${PAYLOAD}" ]; then
    echo "No payload provided. Sending an empty payload."
	PAYLOAD="{}"
fi

POST_DATA="{ \"event_type\": \"${ACTION}\", \"client_payload\": ${PAYLOAD} }"
echo "POST_DATA=${POST_DATA}"

curl --fail -v POST https://api.github.com/repos/${REPO}/dispatches \
-H 'Accept: application/vnd.github.everest-preview+json' \
-H "Authorization: token ${TOKEN}" \
--data "${POST_DATA}"
