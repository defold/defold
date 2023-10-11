## PR checklist

* [ ] Code
	* [ ] Add engine and/or editor unit tests.
	* [ ] New and changed code follows the overall code style of existing code
	* [ ] Add comments where needed
* [ ] Documentation
	* [ ] Make sure that API documentation is updated in code comments
	* [ ] Make sure that manuals are updated (in github.com/defold/doc)
* [ ] Prepare pull request and affected issue for automatic release notes generator
	* [ ] Pull request - Write a message that explains what this pull request does. What was the problem? How was it solved? What are the changes to APIs or the new APIs introduced? This message will be used in the generated release notes. Make sure it is well written and understandable for a user of Defold.
	* [ ] Pull request - Write a pull request title that in a sentence summarises what the pull request does. Do not include "Issue-1234 ..." in the title. This text will be used in the generated release notes.
	* [ ] Pull request - Link the pull request to the issue(s) it is closing. Use on of the [approved closing keywords](https://docs.github.com/en/issues/tracking-your-work-with-issues/linking-a-pull-request-to-an-issue).
	* [ ] Affected issue - Assign the issue to a project. Do not assign the pull request to a project if there is an issue which the pull request closes.
	* [ ] Affected issue - Assign the "breaking change" label to the issue if introducing a breaking change.
	* [ ] Affected issue - Assign the "skip release notes" is the issue should not be included in the generated release notes.

----------

Example of a well written PR description:

1. Start with the user facing changes. This will end up in the release notes.
1. Add one of the GitHub approved closing keywords
1. Optionally also add the technical changes made. This is information that might help the reviewer. It will not show up in the release notes. Technical changes are identified by a line starting with one of these:
   1. `### Technical changes` 
   1. `Technical changes:`
   2. `Technical notes:`

```
There was a anomaly in the carbon chroniton propeller, introduced in version 8.10.2. This fix will make sure to reset the phaser collector on application startup.

Fixes #1234

### Technical changes
* Pay special attention to line 23 of phaser_collector.clj as it contains some interesting optimizations
* The propeller code was not taking into account a negative phase.
```
