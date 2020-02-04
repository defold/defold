# Release guide

## Release branches/channels
* Alpha - git branch: dev
* Beta - git branch: beta
* Stable - git branch: master

## Alpha
Alpha channel is automatically released when [cr-editor-dev](http://ci.defold.com/builders/cr-editor-dev) is built on build bot.

## Beta
Important: *Make sure your branches are up to date!*

 1. If there is a pending Native Extension server change, [publish the stage server](https://github.com/defold/extender#releasing), which updates https://build-stage.defold.com

 1. Make sure dev builds are green

    Check the [Defold SDK Tests](https://github.com/defold/test-sdk) to test that the alpha release still works as expected

 1. Make sure dev is up to date:

        $ git checkout dev
        $ git pull

 1. Merge dev into beta;

        $ git checkout beta
        $ git pull
        $ git merge dev

    (The merge changed to a specific SHA1 commit of dev.)

 1. Push to beta

        $ git push

    This will trigger the beta channel to be built on build bot.

 1. Wait for [builds](https://github.com/defold/defold/actions) to finish, make sure they are green.
 1. (Optional) Download and run beta:

    http://d.defold.com/editor2/`BETA-SHA1`/editor2/Defold-x86_64-darwin.dmg

    http://d.defold.com/editor2/`BETA-SHA1`/editor2/Defold-x86_64-win32.zip

    http://d.defold.com/editor2/`BETA-SHA1`/editor2/Defold-x86_64-linux.zip

    http://d.defold.com/archive/`BETA-SHA1`/beta/editor/Defold-macosx.cocoa.x86_64.dmg

    http://d.defold.com/archive/`BETA-SHA1`/beta/editor/Defold-win32.win32.x86.zip

    http://d.defold.com/archive/`BETA-SHA1`/beta/editor/Defold-linux.gtk.x86_64.zip

    The SHA1 can be found using `git log` on the beta branch.

 1. (Optional) Verify new features and bug fixes.
 1. (Optional) Verify dev mobile apps.

 1. If everything is OK, time to release beta:

    $ `./scripts/build.py release --channel=beta --branch=beta`

    Important: *Make sure the SHA1 and channel is correct!*

 1. Write release beta release notes (while CI builds the test apps)

 1. Verify release by updating an old editor, OSX, Win and Linux.

 1. Post beta release notes on the forum


## Stable

 1. Switch to master branch, merge from beta:

        $ git checkout master
        $ git pull
        $ git merge beta

 1. Push master!

        $ git push

    This will trigger a build of the engines and editors for stable.
    Make a note of the release sha1 (the latest commit to the master branch on GitHub)

 1. Fetch editor via:

    http://d.defold.com/editor2/`STABLE-SHA1`/editor2/Defold-x86_64-darwin.dmg

    http://d.defold.com/editor2/`STABLE-SHA1`/editor2/Defold-x86_64-win32.zip

    http://d.defold.com/editor2/`STABLE-SHA1`/editor2/Defold-x86_64-linux.zip

    http://d.defold.com/archive/`STABLE-SHA1`/stable/editor/Defold-macosx.cocoa.x86_64.dmg

    http://d.defold.com/archive/`STABLE-SHA1`/stable/editor/Defold-win32.win32.x86.zip

    http://d.defold.com/archive/`STABLE-SHA1`/stable/editor/Defold-linux.gtk.x86_64.zip

 1. Tag the release in git:

        $ git tag -a X.Y.Z (same as version produced by the bump)
    Use tag message: Release X.Y.Z

        $ git push origin --tags
    This will push the tags to github, which is then used later in the release step.

### Publishing Stable Release

1. If everything is OK, time to release stable
1. If there is a pending Native Extension server change, [publish the production server](https://github.com/defold/extender#releasing), which updates https://build.defold.com
1. Next, release the stable engine/editor:

        $ ./scripts/build.py release
    Important: *Make sure the SHA1 and channel is correct!*

1. Verify release by updating an old editor, OSX, Win and Linux. (Currently a bit deprecated workflow, since no one uses editor-stable)
1. [Generate](https://github.com/defold/defold.github.io) new API documentation and other documentation changes
1. Post release notes on [forum.defold.com](https://forum.defold.com)

1. Merge master into dev

        $ git checkout dev
        $ git pull
        $ git merge master

1. Bump version:

        $ ./scripts/build.py bump
        $ git diff
        $ git add VERSION
        $ git commit
        > Message: "Bumped version to 1.2.xx"
        $ git push

1. Tell the editor team to update and release the Editor 2
