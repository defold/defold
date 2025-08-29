#!/usr/bin/env bash

SCRIPT_DIR=$(dirname "$(realpath "$0)")")

nix-shell "$SCRIPT_DIR/shell.nix"
