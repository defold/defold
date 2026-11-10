* To update Skribidi, run the script like so:

    > ./update_skribidi.sh <sha1>

* Also update the Skribidi version in `EXTERNAL_PACKAGE_VERSIONS` in `scripts/build.py`.

* Preserve the local `skb_layout.c` fix that selects a font for a complete emoji grapheme before falling back per codepoint, until it is included upstream.

