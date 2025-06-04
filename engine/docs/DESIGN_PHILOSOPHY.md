# Design philosophy

Here we describe our guiding principles of how we reason about code changes.

## Code

One of our goals is to keep the engine as small and performant as possible.
By this we mean both development time, and actual runtime for the released game:
compiled code size, source code size, compile time, runtime.

By keeping this in mind whenever we add new features, we can keep this goal with minimal effort.

### When to add code?

The fastest code is the code that isn't run, or isn't even added!

If we don't need it, don't add it.

Think of _all_ the end users. Will the fix/feature have a clear benefit to most of our users?
If not, can the feature be added as a separate native extension instead?

#### Fixes

For the simple bug fixes, it's usually enough to add a pull request, with little to no design phase beforehand.

Comment the pull request well, and the issues it fixes.

#### Features

Before adding code to the engine, we must have a design first.

Catching problems in a design review is a huge time saver, and it also improves everyones understanding of the code and problem area.

##### Design format

For bigger changes, we need to see a design that outlines the problem, and some possible solutions.
Each solution should have its pros and cons listed, so that reviewers can reason about them.

* Keep it short
    * The document doesn't need to be fancy, or long. Aim for 1-2 pages, as long as it is easy to read and understand for the reviewers.

* Keep it on point
    * The design should only deal with the actual problem. It is easy to think in too generic terms.
The design review will also help highlight such issues.

Although we recommend starting with a design review, to get a first go-ahead, it is sometimes required to do some test
code to see if the idea pans out or not. Be aware that the community might have already touched this idea beforehand
and discarded it for one reason or another. So try to ask first, before spending too much time implementating the feature.

*Note: we still don't have a good shared place to store the design documents, where users can comment on it.
Our current best recommendation is to share a google drive document. If you have a good alternative, let us know!*

#### Backwards compatibility

Even for the least complex fixes, there might be nuances that aren't obvious at first glance.
Be aware that there are many projects out there, and we expect them to build with new versions of Defold.

Sometimes we can argue a breaking change is a bug fix, and we go ahead with the implementation.
In other cases, we need to respect the backwards compatibility.

If uncertain, ask the community for design input.

#### Incremental changes

If your feature is very large, it makes sense to break it down into smaller tasks.
This also gives other developers time to think more properly about your design of a bigger feature.


