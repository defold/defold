#!/usr/bin/env bash

PATCH=to-main.patch

echo "Writing patch ${PATCH}"

git diff --ignore-space-at-eol upstream/dev..HEAD ':!*private.py' ':!*remove_private_files.sh' ':!*private.yml' ':!*debug.appmanifest' ':!com.dynamo.cr*' ':!*nx64*' ':!*meta.*' ':!*switch*.sh' ':!switch*' > ${PATCH}

echo "Now, double check the patch to make sure it doesn't contain any material under NDA"
