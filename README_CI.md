# Continuous Integration

CI is based on [GitHub Actions](https://github.com/features/actions). Current and old jobs can be seen on the [Actions page](https://github.com/defold/defold/actions) of the main Defold repository.

The Defold CI jobs are divided into three main categories, each represented by a separate GitHub Actions Workflow:

* [Editor Only](/.github/workflows/editor-only.yml) - Builds editor feature branches (branches starting with `DEFEDIT-`)
* [Main](/.github/workflows/main-ci.yml) - Builds and tests changes to all other branches. The workflow varies slightly depending on the type of branch being built (dev, beta, master or a feature branch).
* [Engine Nightly](/.github/workflows/engine-nightly.yml) - Runs Address Sanitizer (ASAN) and Valgrind nightly to detect leaks and other problems. This is done on the `dev` branch.

The workflow files listed above sets up the jobs and distributes them to multiple workers to build, test and release the engine and/or editor. The bulk of the work is done in the [ci.py](/ci/ci.py) script.
