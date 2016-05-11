#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Reset local directory.
LOCAL_BASE_DIR="$SCRIPT_DIR/../local"
if [ -d "$LOCAL_BASE_DIR" ]; then
    rm -rf "$LOCAL_BASE_DIR"
fi
mkdir "$LOCAL_BASE_DIR"

# Create repository directory.
LOCAL_REPO_DIR="$LOCAL_BASE_DIR/repo"
mkdir "$LOCAL_REPO_DIR"

# Create template repository.
LOCAL_REPO_TEMPLATE_DIR="$LOCAL_REPO_DIR/template_empty"
mkdir "$LOCAL_REPO_TEMPLATE_DIR"
cd "$LOCAL_REPO_TEMPLATE_DIR"
git init --bare

# Create a first commit with project file and push to template repo
WORKING_DIR="$LOCAL_REPO_TEMPLATE_DIR/../working"
git clone "$LOCAL_REPO_TEMPLATE_DIR" "$WORKING_DIR"
touch "$WORKING_DIR/game.project"
git -C "$WORKING_DIR" add .
git -C "$WORKING_DIR" commit -m "Added game.project file."
git -C "$WORKING_DIR" push origin master
rm -rf "$WORKING_DIR"
