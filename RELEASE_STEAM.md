# Release to Steam

Deploying a release to Steam requires the Steam command line tool `steamcmd`. Authentication is performed using Multi-Factor Authentication (MFA) through Steam Guard.

## Setup

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

1. Update the Steam Community page with the release notes. Create a new post with the relase notes [using this link](https://steamcommunity.com/games/1365760/partnerevents/edit/). Write something like:

```
Defold 1.4.1 has been released
This new version can be downloaded by opening Defold and allowing the editor to self-update.

All releases can be downloaded at: https://github.com/defold/defold/releases
Official forum thread with complete update notes: https://forum.defold.com/t/defold-1-4-1-has-been-released/72246
```
