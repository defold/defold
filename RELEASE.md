# Release guide

## Release branches/channels
* Alpha - git branch: dev
* Beta - git branch: beta
* Stable - git branch: master

## Alpha
The alpha channel is automatically released for every successful push to dev.

## Beta

* If there is a pending Native Extension server change, [publish the stage server](https://github.com/defold/extender/blob/dev/README.md#releasing-stage-server), which updates https://build-stage.defold.com.

1. Merge `dev` into `beta`

        $ git checkout beta
        $ git pull
        $ git merge dev
        $ git push

    Beta channel is automatically released for every successful push to beta.

1. After the beta CI release succeeds, post the generated markdown release notes on [forum.defold.com](https://forum.defold.com/c/releasenotes) and add the "BETA" tag to the headline. Use `releasenotes/X.Y.Z.md` from the CI `release-notes` artifact or from a local manual generation. See [Release Notes](#Release notes) for the URL.

1. Bump version on `dev`:

        $ git checkout dev
        $ ./scripts/build.py bump
        $ git diff
        $ git add VERSION
        $ git commit -m "Bumped version to X.Y.Z"
        $ git push

### Update private repos

1. Also update the beta for the console platforms

        $ cd defold-platform
        $ git checkout dev
        $ git pull

        $ git checkout beta
        $ git pull
        $ git merge dev
        $ git fetch upstream
        $ git merge upstream/beta
        $ git push

## Stable

1. If there is a pending Native Extension server change, [publish the production server](https://github.com/defold/extender#releasing), which updates https://build.defold.com

1. Switch to master branch, merge from beta:

        $ git checkout master
        $ git pull
        $ git merge beta

1. Push master!

        $ git push

    This will trigger a build of the engines and editors for stable.
    Make a note of the release sha1 (the latest commit to the master branch on GitHub)

    * The build will be tagged and published to S3 and to [GitHub Releases](https://github.com/defold/defold/releases)

    * The release notes will be uploaded to S3 before the channel update file is published

    * The refdoc will be updated in the [defold.github.io](https://github.com/defold/defold.github.io) repo

1. Merge `master` into `dev`:

        $ git checkout dev
        $ git pull
        $ git merge master -m "Merged master into dev"
        $ git push

    After a successful build, the editors are published under the stable engine tag in [GitHub Releases](https://github.com/defold/defold/releases)

1. Post the release notes on the [forum](https://forum.defold.com/) and remove the "BETA" part from the headline

1. Announce the Stable release in other channels:

* Twitter (https://x.com/defold)
* LinkedIn (https://www.linkedin.com/company/53400322/admin/page-posts/published/?share=true)
* Discord (usually done by Amel)
* Telegram (usually done by Alexey)
* WebGameDev Discord Native Engines channel (https://discord.com/channels/1032873609280106566/1052862658954670120)
* Poki Discord (https://discord.com/channels/466173880751357963/940949540012433488)

### Update private repos

1. Update the master branch for the console platforms

        $ cd defold-platform
        $ git checkout beta
        $ git pull
        $ git checkout master
        $ git pull
        $ git merge beta

        $ git fetch upstream
        $ git merge upstream/master
        $ git push


1. Update the dev branch for the console platforms

        $ cd defold-platform
        $ git checkout master
        $ git pull
        $ git checkout dev
        $ git pull
        $ git merge master
        $ git fetch upstream
        $ git merge upstream/dev
        $ git push


## Release stable to other stores

* Steam - Follow instructions [here](/RELEASE_STEAM).
* Epic Game Store - Follow instructions [here](/RELEASE_EGS).
* itch.io - Configured with an external link to the latest stable releases on GitHub

## Release notes

Release notes are generated automatically by CI for the `alpha`, `beta` and `stable` channels.

The S3 upload writes:

* `editor2/channels/<channel>/release-notes/<version>.json` - structured notes used by the editor update dialog
* `editor2/channels/<channel>/release-notes/<version>.md` - human-readable notes
* `editor2/channels/<channel>/release-notes/manifest.json` - newest-first version list used by the editor to find notes for skipped versions

Example full URL:

`https://d.defold.com/editor2/channels/alpha/release-notes/1.13.2.json`

Missing release notes fail `beta` and `stable` releases. `alpha` release notes are best-effort; alpha can ship without them when there is no matching GitHub project board yet.

### Manual release notes generation

Use this path to prepare manual override files, rerun generation locally, or recover from a CI/S3 publishing problem.

1. Generate the notes:

        $ export SERVICES_GITHUB_TOKEN=<token>
        $ python scripts/releasenotes_github_projectv2.py --version X.Y.Z --channel beta --token "$SERVICES_GITHUB_TOKEN" generate

    Use `--channel alpha`, `--channel beta` or `--channel stable`. The channel decides the release announcement link and the branch used for commit auditing (`dev`, `beta` or `master`).

    In a shallow checkout, or when local branch history is incomplete, add `--use-github-compare` so commit auditing uses the GitHub compare API:

        $ python scripts/releasenotes_github_projectv2.py --version X.Y.Z --channel beta --token "$SERVICES_GITHUB_TOKEN" --use-github-compare generate

1. Review `releasenotes/X.Y.Z.md` and `releasenotes/X.Y.Z.json`. Both files must exist together. The markdown file is the source to use for forum announcements; the JSON file is what the editor consumes. Commit both files before pushing if they should override CI generation.

1. To manually upload to S3:

        $ python build_tools/releasenotes.py --version X.Y.Z --channel beta

    Use `--archive-domain <domain>` to override the default `DM_ARCHIVE_DOMAIN`/`d.defold.com`. The upload command requires the usual S3 credentials from `~/.s3cfg`, `~/.aws/credentials`, or `S3_ACCESS_KEY` and `S3_SECRET_KEY`.
