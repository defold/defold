
# Release guide

## Release branches/channels
* Alpha - git branch: dev
* Beta - git branch: beta
* Stable - git branch: master

## Alpha
Alpha channel is automatically released when `cr-editor-dev` is built on build bot.

## Beta
Important: *Make sure your branches are up to date!*

 1. Merge dev into beta;

        $ git checkout dev
        $ git pull
        $ git checkout beta
        $ git pull
        $ git merge dev
        $ git push

    (The merge changed to a specific SHA1 commit of dev.)

    This will trigger the beta channel to be built on build bot.

 2. Wait for `cr-editor-beta` to finish, make sure autobuilders are green.
 3. Write release beta release notes.
 4. Download and run beta: http://d.defold.com/archive/`BETA-SHA1`/beta/editor/Defold-macosx.cocoa.x86_64.zip
    The SHA1 can be found in the latest `cr-editor-beta` build, or `git log` on beta branch.

 5. Verify new features and bug fixes.
 6. Verify dev mobile apps.

 7. If everything is OK, time to release beta:
    $ `git./scripts/build.py release --channel=beta --branch=beta`

    Important: *Make sure the SHA1 and channel is correct!*

 8. Verify release by updating an old editor, OSX, Win and Linux.

## Smoke Tests of Beta

### QRT
When the beta has been released the following apps needs to be bundled and sent to QRT:
* Defold Examples - iOS, Android
* Defold IAP Tester - iOS, Android (Google Play and Amazon)
* "Geometry Wars" - iOS, Android
* BBS - iOS, Android
* Presto - iOS, Android

### Defold Team
The following smoke tests are currently performed by the team on each platform (OSX, Win, Linux):
* Start editor, build BBS for desktop and HTML5

## Stable

 1. Switch to master branch, merge from beta:

        $ git checkout master
        $ git pull
        $ git merge beta

 2. Bump version:

        $ ./scripts/build.py bump
        $ git diff
        $ git add VERSION
        $ git commit
        > Message: "Bumped version to 1.2.xx"

 3. Push master!

    $ git push

    This will trigger a build of the engines and editors for stable.
    Check hash for new release via waterfall build(cr-editor)/or latest commit to master on github.

 4. Fetch editor via: http://d.defold.com/archive/`STABLE-SHA1`/stable/editor/Defold-macosx.cocoa.x86_64.zip
 5. Tag the release in git:

        $ git tag -a X.Y.Z (same as version produced by the bump)
    Use tag message: Release X.Y.Z

        $ git push origin --tags
    This will push the tags to github, which is then used later in the release step.

### Publishing Stable Release

1. If everything is OK, time to release stable:

        $ ./scripts/build.py release
    Important: *Make sure the SHA1 and channel is correct!*

2. Verify release by updating an old editor, OSX, Win and Linux.
3. Post release notes on forum.defold.com and send notification email to defold-users@king.com
4. Merge master into dev

        $ git checkout dev
        $ git merge master
        $ git push




