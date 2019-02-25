# Facebook has been moved

We are in the process of moving a few of Defolds features from core
functionality into external native extensions. The goal is to keep the engine
as lean and minimal as possible, moving functionality that might only be
relevant for a select few of users. This also means that the extensions will be
available in full source code, enabling external developers to review and
contribute to them.

Moving the `facebook` module part of this goal, and with the 1.2.Xxx
release of Defold will no longer be part of the core functionality.


## Migrating to the external `facebook` extension

If you previously were using the `facebook` module, you only need to add the
following URL to your [game.project](/game.project) dependencies:
```
https://github.com/defold/extension-facebook/archive/master.zip
```

It has the same API and functionality as before.

The documentation is now available on GitHub:
[https://defold.github.io/extension-facebook/](https://defold.github.io/extension-facebook/)

The source code can be viewed on the public GitHub repository:
[https://github.com/defold/extension-facebook](https://github.com/defold/extension-facebook)
