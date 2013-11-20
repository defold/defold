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

**General Setup**

1. Register an app on Google API. https://code.google.com/apis/console
2. Enable Google Cloud Messaging for Android
3. Create a server-key
4. Get sender-id from url (Project Number)

**Client Setup**

1. Set *game.project* option android.gcm_sender_id to sender-id
2. Use *push.register* and *push.set_listener*
3. See AndroidManifest.xml for all the required elements for push

**Debug**

Run the following command-line to send a push message:

	curl  -X POST  -H "Content-type: application/json"  -H 'Authorization: key=KEY' -d '{"registration_ids" : ["REG_ID"], "data": {"alert": "Hello"}}' https://android.googleapis.com/gcm/send

and replace ***KEY*** with server-key from the Google API Console and ***REG_ID*** with device token returned from *push.register*

**Conventions**

By convention the key *alert* must be available for push notification text in notification manager. If not a default message will be displayed.

**Limitations**

Notifications are not presented in then notification manager when no application icon is specified. Limitations in Android.
