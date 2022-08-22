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

 1. Switch to master branch, merge from beta:

        $ git checkout master
        $ git pull
        $ git merge beta

 1. Push master!

        $ git push

    This will trigger a build of the engines and editors for stable.
    Make a note of the release sha1 (the latest commit to the master branch on GitHub)

1. Also update the master for the console platforms

        $ cd defold-platform
        $ git checkout beta
        $ git pull
        $ git checkout master
        $ git pull
        $ git merge beta

        $ git fetch upstream
        $ git merge upstream/master
        $ git push


 1. Fetch editor via:

    http://d.defold.com/archive/editor-alpha/`STABLE-SHA1`/editor-alpha/editor2/Defold-x86_64-macos.dmg

    http://d.defold.com/editor-alpha/editor2/`STABLE-SHA1`/editor-alpha/editor2/Defold-x86_64-win32.zip

    http://d.defold.com/archive/editor-alpha/`STABLE-SHA1`/editor-alpha/editor2/Defold-x86_64-linux.zip

 1. Tag the release in git:

        $ git checkout master
        $ git tag -a X.Y.Z -m "Release X.Y.Z"

    Add `-f` to force update the tag (in the case you need to amend an existing release for some reason).

        $ git push origin --tags -f

    This will push the tags to GitHub, which is then used later in the release step. Add `-f` to force update the tags.

### Publishing Stable Release

1. If everything is OK, time to release stable.

1. If there is a pending Native Extension server change, [publish the production server](https://github.com/defold/extender#releasing), which updates https://build.defold.com

1. Next, release the stable engine/editor:

        $ git checkout master
        $ ./scripts/build.py release --channel=stable
    Important: *Make sure the SHA1 and channel is correct!*

1. Merge `master` into `editor-dev`:

        $ git checkout editor-dev
        $ git pull
        $ git merge master -m "Merged master into editor-dev"
        $ git push

This will trigger a build of the engines and editors for editor-alpha.

1. When the `editor-dev` is built, all channels have been updated.

1. Verify release by updating an old editor, OSX, Win and Linux.

1. [Generate](https://github.com/defold/defold.github.io) new API documentation and other documentation changes. From the `defold/defold.github.io` repo:

        $ cd defold.github.io
        $ git checkout master
        $ git pull
        $ ./update.py --download refdoc
        $ git commit -am "Updated reference documentation to X.Y.Z"
        $ git push

1. Merge `master` into `dev`:

        $ cd defold
        $ git checkout dev
        $ git pull
        $ git merge master -m "Merged master into dev"

1. Bump version:

        $ ./scripts/build.py bump
        $ git diff
        $ git add VERSION
        $ git commit -m "Bumped version to X.Y.Z"
        $ git push

1. Update the dev branch for the console platforms

        $ cd defold-platform
        $ git checkout dev
        $ git pull
        $ git fetch upstream
        $ git merge upstream/dev
        $ git push


1. Repost the releasenotes on the [forum](https://forum.defold.com/) and remove the "BETA" part from the headline

1. Upload release artifacts to GitHub:

        $ cd defold
        $ git checkout master
        $ ./scripts/build.py --github-token=YOUR_GITHUB_TOKEN release_to_github --channel=stable
