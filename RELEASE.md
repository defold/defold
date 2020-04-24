# Release guide

## Release branches/channels
* Alpha - git branch: dev
* Beta - git branch: beta
* Stable - git branch: master

## Alpha
The alpha channel is automatically released for every successful push to dev.

## Beta
Beta beta channel is automatically released for e very successful push to beta.

* If there is a pending Native Extension server change, [publish the stage server](https://github.com/defold/extender/blob/dev/README.md#releasing-stage-server), which updates https://build-stage.defold.com.
* Collect release notes using `sripts/releasenotes_git.py` and post on [forum.defold.com](https://forum.defold.com/c/releasenotes)

## Stable

 1. Switch to master branch, merge from beta:

        $ git checkout master
        $ git pull
        $ git merge beta

 1. Push master!

        $ git push

    This will trigger a build of the engines and editors for stable.
    Make a note of the release sha1 (the latest commit to the master branch on GitHub)

 1. Merge `master` into `editor-dev`

        $ git checkout editor-dev
        $ git pull
        $ git merge master
        $ git push

    This will trigger a build of the engines and editors for editor-alpha.

 1. When the `editor-dev` is built, all channels have been updated

 1. Fetch editor via:

    http://d.defold.com/editor2/`STABLE-SHA1`/editor2/Defold-x86_64-darwin.dmg

    http://d.defold.com/editor2/`STABLE-SHA1`/editor2/Defold-x86_64-win32.zip

    http://d.defold.com/editor2/`STABLE-SHA1`/editor2/Defold-x86_64-linux.zip

 1. Tag the release in git:

        $ git tag -a X.Y.Z (same as version produced by the bump)

    Use tag message: Release X.Y.Z
    Add `-f` to force update the tag (in the case you need to amend an existing release for some reason).

        $ git push origin --tags

    This will push the tags to GitHub, which is then used later in the release step. Add `-f` to force update the tags.

### Publishing Stable Release

1. If everything is OK, time to release stable

1. If there is a pending Native Extension server change, [publish the production server](https://github.com/defold/extender#releasing), which updates https://build.defold.com

1. Next, release the stable engine/editor:

        $ ./scripts/build.py release
    Important: *Make sure the SHA1 and channel is correct!*

1. Verify release by updating an old editor, OSX, Win and Linux. (Currently a bit deprecated workflow, since no one uses editor-stable)

1. [Generate](https://github.com/defold/defold.github.io) new API documentation and other documentation changes. From the `defold/defold.github.io` repo:

        $ ./update.py --download refdoc
        $ git commit -am "Updated reference documentation to 1.2.xxx"
        $ git push

1. Upload release artifacts to GitHub:

        $ ./scripts/build.py --github-token=YOUR_GITHUB_TOKEN release_to_github


1. Merge `master` into dev

        $ git checkout dev
        $ git pull
        $ git merge master

1. Merge `editor-dev` into `dev`

        $ git merge editor-dev

1. Bump version:

        $ ./scripts/build.py bump
        $ git diff
        $ git add VERSION
        $ git commit
        > Message: "Bumped version to 1.2.xx"
        $ git push

1. Collect release notes using `sripts/releasenotes_git.py` and post on [forum.defold.com](https://forum.defold.com/c/releasenotes)
