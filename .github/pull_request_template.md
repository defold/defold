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
