# Continuous Integration

CI is based on [GitHub Actions](https://github.com/features/actions). Current and old jobs can be seen on the [Actions page](https://github.com/defold/defold/actions) of the main Defold repository.

The Defold CI jobs are divided into three main categories, each represented by a separate GitHub Actions Workflow:

* [Main](/.github/workflows/main-ci.yml) - Builds and tests changes to all branches. The workflow varies slightly depending on the type of branch being built (dev, beta, master or a feature branch).
* [PR - ok to test](/.github/workflows/pr-ok-to-test.yml) - Builds a reviewed external (fork) pull request through `Main`. See [External contributions](#external-contributions) below.
* [Engine Nightly](/.github/workflows/engine-nightly.yml) - Runs Address Sanitizer (ASAN) and Valgrind nightly to detect leaks and other problems. This is done on the `dev` branch.

The workflow files listed above sets up the jobs and distributes them to multiple workers to build, test and release the engine and/or editor. The bulk of the work is done in the [ci.py](/ci/ci.py) script.

## How to trigger builds manually

You can use the `ci/trigger-build.py` script to manually trigger a build using the `Main` workflow:

```
./ci/trigger-build.py --token=<personal_access_token> --branch=9a32ac5e9513e8aff669cf4cbe4334aeec2fbf8e --skip-engine --skip-sdk --skip-bob
```

Available options are:

```
$ ./ci/trigger-build.py --help                                                                   
usage: trigger-build.py [-h] [--token TOKEN] [--action ACTION]
                        [--branch BRANCH] [--skip-engine] [--skip-sdk]
                        [--skip-bob] [--skip-editor]

optional arguments:
  -h, --help       show this help message and exit
  --token TOKEN    GitHub API personal access token
  --action ACTION  The trigger action
  --branch BRANCH  The branch to build
  --skip-engine    Skip building the engine
  --skip-sdk       Skip building the Defold SDK
  --skip-bob       Skip building bob
  --skip-editor    Skip building the editor
```

## External contributions

A pull request from a fork gets no CI: the engine jobs in `main-ci.yml` do not run for
`pull_request` events, and the fork's branch does not exist in this repository to check out.

To build one, a maintainer reviews the diff and applies the **`ok to test`** label.
[PR - ok to test](/.github/workflows/pr-ok-to-test.yml) then:

1. checks that the label was applied by someone with push access, and that the pull request
   targets `dev`,
2. mirrors the exact commit that was reviewed to `contrib/pr-<number>`, with this
   repository's `.github/` in place of the pull request's,
3. dispatches `Main` on that branch, and
4. comments on the pull request with a link to the run and a list of the build-affecting
   files in the diff.

Pushing new commits to the pull request removes the label and deletes `contrib/pr-<number>`;
a maintainer has to review the new commits and apply the label again. Closing the pull
request deletes the branch.

### What a contrib build does differently

Mirroring a fork into this repository removes the trust boundary GitHub normally puts
around fork pull requests: the build executes `wscript`s, `CMakeLists.txt`, Gradle files,
`ci/**`, `scripts/build.py` and the editor's Clojure tasks straight from the pull request.
Log redaction is not a control against that, so a run on `contrib/**` simply does not get
the credentials:

| | Trusted build | Contrib build |
|---|---|---|
| Signing (`GCLOUD_EDITOR_SERVICE_KEY`, `MACOS_CERTIFICATE`, `NOTARIZATION_*`) | yes | **no** |
| S3 (`S3_ACCESS_KEY`, `S3_SECRET_KEY`) | yes | **no** |
| Private repos (`SERVICES_GITHUB_TOKEN`, `DM_RELEASE_REPOSITORY`) | yes | **no** |
| `DM_PACKAGES_URL`, `DM_ARCHIVE_DOMAIN` | yes | yes |
| Actions cache (`actions/cache`, `setup-java`, `get-cmake`) | the branch's own scope | **a scope of its own, and no cache steps at all** |
| Console platforms (Switch, PlayStation, Xbox) | yes | **no** |
| Release notes, release, API ref publish | yes | **no** |
| Slack alarms (`SLACK_WEBHOOK`) | on `dev`/`beta`/`master` | **no**, and the webhook is withheld |
| Engine, bob and editor output | archived to S3 | GitHub Actions artifacts |

Nothing in a contrib run reads a `${{ env.* }}` value in an `if:`. Workflow-level `env:` is
evaluated once into the runner's global environment, and a `run:` step rewrites that same
dictionary through `$GITHUB_ENV` - so a gate that reads it can be flipped by the code being
built. Every trust decision in `main-ci.yml` is keyed on the branch being built:

```
startsWith(github.ref, 'refs/heads/contrib/')
  || startsWith(github.event.client_payload.branch, 'contrib/')
  || startsWith(github.head_ref, 'contrib/')
```

The three terms are the three ways `BUILD_BRANCH` can be set - the ref the run started on,
the branch named by a `repository_dispatch`, and the head branch of a pull request - so a
`contrib/**` branch cannot be built with credentials by arriving through a different door.
All three are part of the event and cannot be changed from inside a job.

### Cache scope

The Actions cache is scoped by `GITHUB_REF`, and a run cannot write outside its own scope -
but neither can it be stopped from writing *inside* it. Every job is handed the runner's own
cache credential, so the code being built can call the cache API directly whether or not the
workflow uses `actions/cache`; leaving the cache steps out keeps nothing out of the cache.
What matters is which scope the run is in.

That is why `Main` is dispatched **on the mirror branch** rather than through a
`repository_dispatch`. A repository dispatch reports `GITHUB_REF` as `refs/heads/dev`
whatever branch it names, which would put every contrib build in the default branch's cache
scope - the one trusted builds restore from. A run on `contrib/pr-<number>` writes to that
branch's scope instead, and GitHub does not let a run on the default branch restore a cache
created for a child branch.

The cache steps are switched off for contrib runs all the same, so nothing depends on that
scoping being right: `~/.dcache`, the lein install and the Maven and Gradle caches all hold
code that later builds execute, and the download caches do not verify a checksum on a hit.
Contrib runs pay a cold build instead.

A second rule follows from the same reasoning: **a run may only write to the cache scope
that belongs to the code it built.** For a push or a dispatch on a branch those are the same
thing, but a `repository_dispatch` always reports `GITHUB_REF` as the default branch while
`BUILD_BRANCH` names whatever branch was asked for - so it builds one branch and caches into
another's scope. Every caching step is therefore also gated on
`github.event_name != 'repository_dispatch'`, including the release job's, which would
otherwise build `beta` or `master` into `dev`'s scope. Builds started by
`ci/trigger-build.py` run cold as a result.

Caching is not only `actions/cache`. An action that caches on its own counts, and the same
two gates apply: `setup-java` is passed `cache: ''`, `lukka/get-cmake` is passed
`useCloudCache: false`, and the shared `defold/github-actions-common` Android action, which
keeps the SDK and NDK tree in the Actions cache, is passed `cache: false`.

Because there is no S3, a contrib build hands its output between jobs as artifacts instead:
the engine jobs upload `$DYNAMO_HOME` (`dynamo-home-<platform>`), `build-bob` restores them
and builds with `--skip-archive`, and the editor jobs build with
`--engine-artifacts=dynamo-home --skip-distclean`. `build-sdk` is skipped, since it is a
merge of the per-platform SDK archives on S3.

Dispatching on the mirror branch runs the workflow file found **on that branch**, so the
mirror is written with this repository's `.github/` spliced in place of the pull request's -
otherwise a pull request could rewrite the CI that builds it. The reviewed commit is kept as
the parent of the mirror commit, so the substitution is one commit's worth of difference and
stays visible in the history. `GITHUB_TOKEN` is allowed to start a dispatch (a push made
with `GITHUB_TOKEN` would start no workflow at all).

Workflow files in the pull request are therefore never executed, but they still have to be
reviewed before merging - the mirror job lists them in its comment.

A pull request that changes CI itself is best built by a maintainer from a branch in this
repository instead.
