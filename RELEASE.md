# Release guide

## Release branches/channels
* Alpha - git branch: dev
* Beta - git branch: beta
* Stable - git branch: master

## Alpha
The alpha channel is automatically released for every successful push to dev.

## Beta

* If there is a pending Native Extension server change, [publish the stage server](https://github.com/defold/extender/blob/dev/README.md#releasing-stage-server), which updates https://build-stage.defold.com.

1. Merge `editor-dev` into `dev`

        $ git checkout editor-dev
        $ git pull
        $ git checkout dev
        $ git pull
        $ git merge editor-dev
        $ git push

1. Merge `dev` into `beta`

        $ git checkout beta
        $ git pull
        $ git merge dev
        $ git push

    Beta channel is automatically released for every successful push to beta.

1. Collect release notes using `python scripts/releasenotes_github_projectv2.py` and post on [forum.defold.com](https://forum.defold.com/c/releasenotes)
and add the "BETA" tag to the headline

* Note: The release notes script requires a github access token to work correctly (https://github.com/settings/tokens). If the token is incorrect, you will likely get an error saying 'Unable to find GitHub project for version x.x.x'. Create a "classic" github token and add permissions to read projects. If the script still fails, you might need more permissions.

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

    * The refdoc will be updated in the [defold.github.io](https://github.com/defold/defold.github.io) repo

1. Merge `master` into `editor-dev`:

        $ git checkout editor-dev
        $ git pull
        $ git merge master -m "Merged master into editor-dev"
        $ git push

    After a successful build, the editors are published under the stable engine tag in [GitHub Releases](https://github.com/defold/defold/releases)

1. Merge `editor-dev` into `dev`:

        $ cd defold
        $ git checkout dev
        $ git pull
        $ git merge editor-dev -m "Merged editor-dev into dev"

1. Repost the releasenotes on the [forum](https://forum.defold.com/) and remove the "BETA" part from the headline

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
