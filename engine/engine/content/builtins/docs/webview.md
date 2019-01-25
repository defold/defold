# WebView has been moved

We are in the process of moving a few of Defolds features from core
functionality into external native extensions. The goal is to keep the engine
as lean and minimal as possible, moving functionality that might only be
relevant for a select few of users. This also means that the extensions will be
available in full source code, enabling external developers to review and
contribute to them.

The `webview` module is the first step towards this goal, and with the 1.2.146
release of Defold will no longer be part of the core functionality.



## Migrating to the external `webview` extension

If you previously were using the `webview` module, you only need to add the
following URL to your [game.project](/game.project) dependencies:
```
https://github.com/defold/extension-webview/archive/master.zip
```

It has the same API and functionality as before.

The documentation is now available on GitHub:
[https://defold.github.io/extension-webview/](https://defold.github.io/extension-webview/)

The source code can be viewed on the public GitHub repository:
[https://github.com/defold/extension-webview](https://github.com/defold/extension-webview)
