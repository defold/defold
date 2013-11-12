Push
====


General
-------


iOS
---

During development make sure that you have an entitlements file with support for push, i.e. aps-environment.
See defold/share/push_profile.xcent for an example. When bundling, using the editor, the entitlement should be
extracted automatically from the provision profile. Extracting entitlement is a feature that could be added to waf.

Android
-------
