# Contribution Guidelines
Thank you for visiting the Defold open source contribution guidelines. Together we will make Defold an even better product!

Before you start contributing we would like to ask you to familiarise yourself with our contribution guidelines. Following these guidelines will make the process of contributing changes a lot smoother and faster.

Thanks!

The Defold team <3

## Code of conduct
We ask of all contributors, project members and users to follow our [Code of Conduct](CODE_OF_CONDUCT) to create an open and welcoming environment. We will remove, edit, or reject comments, commits, code, wiki edits, issues, and other contributions that are not aligned to our Code of Conduct, or to ban temporarily or permanently any contributor for other behaviors that we deem inappropriate, threatening, offensive, or harmful.

## Bug reports
Bugs should be reported to our public issue tracker using the [Bug Report template](https://github.com/defold/defold/issues/new?assignees=&labels=bug&template=bug_report.md&title=). Some guidelines:

* Please [search already reported bug reports](https://github.com/defold/defold/labels/bug) before reporting a bug.
  * If there is an already existing bug report we appreciate if you add additional information to the existing bug report instead of creating a new one.
  * We will close duplicate bug reports.
* If you do create a new Bug Report it is important that you fill in all fields marked as required.
  * We will not be able to consider a new Bug Report until all required fields are filled in.
  * Never submit more than one Bug Report in a single GitHub issue.

## Feature requests
We value your ideas on how to improve Defold! You can submit feature requests to our public issue tracker using the [Feature Request template](https://github.com/defold/defold/issues/new?assignees=&labels=feature+request&template=feature_request.md&title=). Some guidelines:

* Please [search already submitted feature requests](https://github.com/defold/defold/labels/feature_request) before submitting your feature request. Maybe someone has already suggested the same thing or something only slightly different?
  * If there is an already submitted feature request we appreciate if you add additional information instead of creating a new feature request.
  * We will close any duplicate feature requests.
* If you do submit a new Feature Request it is important that you fill in all fields marked as required.
  * We will not be able to consider a new Feature Request until all required fields are filled in.
  * Never submit more than one Feature Request in a single GitHub issue.

## Documentation
The Defold manuals and examples are hosted in a [separate repository](https://github.com/defold/doc). Please pay it a visit if you wish to help improve our documentation! We appreciate all contributions, be them fixed typos, updated images, new tutorials or clarifications on existing content.

Note that the API reference is generated from the source code of the Defold engine. If you wish to submit improvements to the API reference it should be done as Pull Requests (see below).

## Pull requests (PRs)
Submit pull requests to contribute to the Defold engine and/or editor source code. Pull requests can introduce new features, fix bugs or improve on our API reference. Pull Requests for new features and bug fixes must follow these guidelines:

* Never submit a PR that does more than one thing.
* You must sign our Contributor License Agreement (CLA) as part of submitting the PR.
  * The CLA ensures that you are entitled to contribute the code/documentation/translation to the project you’re contributing to and are willing to have it used in distributions and derivative works.
  * The process of digitally signing the CLA is integrated into the PR flow on GitHub.
* Always reference existing issues covered by the PR.
* Individual commits should have informative commit messages.
* For new code or changes to existing code:
  * Should be covered by tests (unit, integration or system tests).
    * All tests must pass CI before a Pull Request is approved and can be merged.
  * Must follow the defined coding style.
    * Use the .clang_format file for engine code.
  * Should if possible be tested on all target platforms (preferably on physical hardware).
    * Please note in the PR on which platforms the change has been tested.
* All pull requests must be made towards the defold:dev branch for changes to the engine and defold:editor-dev for changes to the editor.

### PR - New features
If you want to add new functionality there are several additional steps that must be taken before you can submit a pull request:

* New functionality must be well designed and useful to not only your own project.
  * For game engine code there is always the option to create a native extension and share it via the Asset Portal if the functionality is specific to a certain type of project or in other ways not suitable for inclusion in the engine.
* You should start by creating a [Feature Design ticket](https://github.com/defold/defold/issues/new?assignees=&labels=feature_design&title=) to discuss the proposed solution.
  * The Feature Design should discuss the pros and cons of different solutions and it should document why a decision is made to implement the feature in a certain way.
  * Never submit a PR without first creating a design ticket.
  * Pull requests for new features without a corresponding GitHub issue will not be approved.
  * Reference any existing Feature Request covered by the design.
  * Any new API that is introduced should be well designed and future proof as we always try to avoid introducing breaking changes.
    * New engine functionality that breaks existing functionality should be avoided at all costs.
  * When the final design has been agreed upon the PR will be locked from further updates.
* Reference the Feature Design in the PR.

### PR - API reference
The API reference is automatically extracted from the source code as a separate build step. There are no additional considerations to be made for PRs to the documentation besides paying attention to the grammar and making sure that the changes correct and/or clarify existing documentation.
