# Release to Epic Games Store

## Setup

1. [Install](https://dev.epicgames.com/docs/epic-games-store/publishing-tools/uploading-binaries/bpt-instructions-170) `BuildPatchTool` on your local machine. 

1. Export the EGS Client Secret (Dev Portal under Your Product > Product Settings > General tab > Build Patch Tool Credentials) as an environment variable named `DEFOLD_EGS_BPT_CLIENT_SECRET`

## Release

1. From the checkout for the published stable release, confirm that [VERSION](VERSION) names the version you want to upload, then run `./scripts/build.py release_to_egs`. To select a different published version, pass it with `--version`.
