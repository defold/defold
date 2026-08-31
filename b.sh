#!/usr/bin/env bash

./scripts/build.py build_engine --platform=arm64-macos --skip-docs --skip-tests -- --opt-level=0 --skip-build-tests

./scripts/build.py build_bob --skip-tests
