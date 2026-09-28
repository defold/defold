# Defold repository guidance

## Area guidance

Before working in an area or editing its shared build scripts, read the relevant guidance:

- [Engine guidance](engine/AGENTS.md) for engine source, builds, and tests.
- [Editor guidance](editor/AGENTS.md) for editor development.
- [Bob guidance](com.dynamo.cr/com.dynamo.cr.bob/AGENTS.md) for Bob development and `bob.jar` builds.

## License headers

Add the full Defold License header to new first-party source files. When editing a first-party source with no header or an abbreviated Defold header, add the full header. Preserve third-party notices. Place the header before code (after a shebang, if present), use the file's comment syntax, and update the Defold Foundation end year to the current year. The complete text, without comment markers, is:

```txt
Copyright 2020-2026 The Defold Foundation
Copyright 2014-2020 King
Copyright 2009-2014 Ragnar Svensson, Christian Murray
Licensed under the Defold License version 1.0 (the "License"); you may not use
this file except in compliance with the License.

You may obtain a copy of the License, together with FAQs at
https://www.defold.com/license

Unless required by applicable law or agreed to in writing, software distributed
under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied. See the License for the
specific language governing permissions and limitations under the License.
```

## Changes and tests

- Add focused tests for behavior changes. Put a comment immediately above each new test declaration, outside the test body, stating what it verifies and the regression it guards against, if any.
- Keep each PR focused on one problem; target `defold:dev` unless instructed otherwise. Link related issues, describe the behavior change, report validation and tested platforms, and include screenshots for editor UI changes.
