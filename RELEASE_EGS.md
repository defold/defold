# Release to Epic Games Store

## Setup

1. [Install](https://dev.epicgames.com/docs/epic-games-store/publishing-tools/uploading-binaries/bpt-instructions-170) `BuildPatchTool` on your local machine. 

1. Export the EGS Client Secret (Dev Portal under Your Product > Product Settings > General tab > Build Patch Tool Credentials) as an environment variable named `DEFOLD_EGS_BPT_CLIENT_SECRET`

## Release

1. Run `release_to_egs` command:

        $ ./scripts/build.py --version=1.4.8 release_to_egs
