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
   targets `dev`, `beta` or `master`,
2. mirrors the exact commit that was reviewed to `contrib/pr-<number>`,
3. fires a `repository_dispatch` of type `contrib-build` for that branch, which `Main` picks
   up, and
4. comments on the pull request with a link to the run and a list of the build-affecting
   files in the diff.

Pushing new commits to the pull request removes the label and deletes `contrib/pr-<number>`;
a maintainer has to review the new commits and apply the label again. Closing the pull
request deletes the branch.

### What a contrib build does differently

Mirroring a fork into this repository removes the trust boundary GitHub normally puts
around fork pull requests: the build executes `wscript`s, `CMakeLists.txt`, Gradle files,
`ci/**`, `scripts/build.py` and the editor's Clojure tasks straight from the pull request.
Log redaction is not a control against that, so a `contrib-build` run simply does not get
the credentials:

| | Trusted build | Contrib build |
|---|---|---|
| Signing (`GCLOUD_EDITOR_SERVICE_KEY`, `MACOS_CERTIFICATE`, `NOTARIZATION_*`) | yes | **no** |
| S3 (`S3_ACCESS_KEY`, `S3_SECRET_KEY`) | yes | **no** |
| Private repos (`SERVICES_GITHUB_TOKEN`, `DM_RELEASE_REPOSITORY`) | yes | **no** |
| `DM_PACKAGES_URL`, `DM_ARCHIVE_DOMAIN` | yes | yes |
| Actions cache (`actions/cache`, `setup-java`, `get-cmake`) | yes | **no cache steps at all** |
| Console platforms (Switch, PlayStation, Xbox) | yes | **no** |
| Release notes, release, API ref publish | yes | **no** |
| Slack alarms (`SLACK_WEBHOOK`) | on `dev`/`beta`/`master` | **no**, and the webhook is withheld |
| Engine, bob and editor output | archived to S3 | GitHub Actions artifacts |

Nothing in a contrib run reads a `${{ env.* }}` value in an `if:`. Workflow-level `env:` is
evaluated once into the runner's global environment, and a `run:` step rewrites that same
dictionary through `$GITHUB_ENV` - so a gate that reads it can be flipped by the code being
built. Every trust decision in `main-ci.yml` is keyed on `github.event.action`, which is
part of the event and cannot be changed from inside a job.

Caching is switched off rather than renamed. The Actions cache is scoped by `GITHUB_REF`
alone, and `GITHUB_REF` is `refs/heads/dev` for *any* `repository_dispatch`, so a contrib
job sits in the default branch's cache scope. A key prefix only changes the name of what
gets written there; it is not an access control. Since `~/.dcache`, the lein install and
the Maven and Gradle caches all hold code that later builds execute, and the download
caches do not verify a checksum on a hit, a poisoned entry would run in a job that does
hold the signing keys. So contrib runs get no cache steps, and pay a cold build instead.

An action that caches on its own counts as a cache step: `lukka/get-cmake` is passed
`useCloudCache: false`, and the shared `defold/github-actions-common` Android action, which
keeps the SDK and NDK tree in the Actions cache, is passed `cache: false`.

Because there is no S3, a contrib build hands its output between jobs as artifacts instead:
the engine jobs upload `$DYNAMO_HOME` (`dynamo-home-<platform>`), `build-bob` restores them
and builds with `--skip-archive`, and the editor jobs build with
`--engine-artifacts=dynamo-home --skip-distclean`. `build-sdk` is skipped, since it is a
merge of the per-platform SDK archives on S3.

The dispatch is deliberate: GitHub runs a `repository_dispatch` workflow from the **default
branch**, so a pull request can never modify the CI definition that builds it, and
`GITHUB_TOKEN` is allowed to fire one (a push made with `GITHUB_TOKEN` would start no
workflow at all). Workflow files in the pull request are therefore inert during the build,
but they still have to be reviewed before merging - the mirror job lists them in its comment.

A pull request that changes CI itself is best built by a maintainer from a branch in this
repository instead.
