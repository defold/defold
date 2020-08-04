# How to integrate back to the dev branch


## Update the switch-to-dev branch

    $ git checkout switch-to-dev
    $ git merge dev

## Merge the platform-switch branch

Leave the files uncommitted, since we wish to revert some files:


    $ git merge --no-commit --no-ff platform-switch


Revert the files:

    $ ./scripts/revert_private_files.sh