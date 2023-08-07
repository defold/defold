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

    Beta beta channel is automatically released for every successful push to beta.

1. Collect release notes using `python scripts/releasenotes_git.py` and post on [forum.defold.com](https://forum.defold.com/c/releasenotes)
and add the "BETA" tag to the headline

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

1. Bump version:

        $ ./scripts/build.py bump
        $ git diff
        $ git add VERSION
        $ git commit -m "Bumped version to X.Y.Z"
        $ git push

1. Repost the releasenotes on the [forum](https://forum.defold.com/) and remove the "BETA" part from the headline

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
        $ git checkout dev
        $ git pull
        $ git fetch upstream
        $ git merge upstream/dev
        $ git push


## Release to Steam

Deploying a release to Steam requires the Steam command line tool `steamcmd`. Authentication is performed using Multi-Factor Authentication (MFA) through Steam Guard.

### Setup

For manual release to Steam:

1. Install `steamcmd` on your local machine. You can either install using `brew install steamcmd` or by [downloading from Valve](https://partner.steamgames.com/doc/sdk/uploading#1) and adding `tools/ContentBuilder/builder_osx/steamcmd` to your path.

1. Try to login with `steamcmd +login <username> <password> +quit`, which may prompt for the MFA code. If so, type in the MFA code that was emailed to the account's email address.

1. Validate that the MFA process is complete by running `steamcmd +login <username> +quit` again. It should not ask for the MFA code again.


For automated release on CI (NOT ENABLED YET):

1. After running `steamcmd` a `config.vdf` file will be created. The location varies depending on operating system. On macOS the file is located in `~/Library/Application Support/Steam/config`. 

1. Use `cat config.vdf | base64 > config_base64.txt` to encode the file. Copy the contents of `config_base64.txt` to the GitHub Secret `STEAM_CONFIG_VDF`.


NOTE: If when running the action you recieve another MFA code via email, run `steamcmd +set_steam_guard_code <code>` on your local machine and repeat the `config.vdf` encoding and replace secret `STEAM_CONFIG_VDF` with its contents.


## Manual release

1. Run `release_to_steam` command:

        $ ./scripts/build.py --version=1.4.8 release_to_steam

1. Wait until command completes.

1. Set build live (see below).


## Automated release (NOT ENABLED YET)

1. Commit code to master

1. Wait for CI.

1. Set build live (see below).


## Set build live

When the build has been uploaded it needs to be set live:

1. Login to [https://partner.steamgames.com](https://partner.steamgames.com)

1. Open build list for Defold: [https://partner.steamgames.com/apps/builds/1365760](https://partner.steamgames.com/apps/builds/1365760)

1. Find your upload in the list and change the value in the `Set build live on branch` to `default`



