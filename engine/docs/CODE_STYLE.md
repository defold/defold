# Code Style and Standards

## Formatting

Follow current code style as defined in `.clang-format`, or follow the existing style of the file you're editing.

## C-like C++

_We also have a detailed blog post about our [code style](https://defold.com/2020/05/31/The-Defold-engine-code-style/)_

Try to stay as close to C style programming as possible.
We _do_ however use a few features of C++:

* Namespaces
* RAII (in a few places, e.g. profile scopes)
* Templates (in some of our containers, and a few functions)
* Constructors of `struct` (as few as possible)

*We don't use classes, although we do on occasion use structs with constructors (which is more or less a class, with slightly less code).*

Keep these to a minimum.
In short, if we don't need it, we don't use it.

## C++ version

Although we don't actively specify a C++ version, we aim for a very simple feature set that should work across compilers.
Essentially, the engine code use no C++ version higher than C++98.

Do not add "modern C++" features such as `auto` or lambdas. We avoid them for readability reasons.

## No C++ Exceptions

We don’t make use of any exceptions in the engine. It isn’t generally used in game engines, since the data is (mostly) known beforehand, during development. Removing the support for C++ exceptions decreases executable size and improves the runtime performance.

## No STL

While it might be tempting to use `std` functions/types/classes, we don't do it for several reasons.
Performance is one (code size, compilation time, debugging time), and ABI issues is another important issue.

You can read a bit more about the advice we give to extension developers here:
https://defold.com/manuals/extensions-best-practices/#standard-template-libraries---stl

We do make a (very) few exceptions, most notably for `std::sort()`.
Although eventually, it may be replaced too.

## Memory allocations

Most often we try to allocate memory upfront, when loading resources (e.g. loading a `.collection` file).
We try to avoid runtime allocations as much as possible.

We often use `free`, `malloc` and `realloc` to emphasize that the structs doesn't need a constructor.

## Avoid defensive programming

Avoid adding defensive code such as `if (ptr != 0) *ptr = ...` as much as possible.
Checks for validity should be done as early as possible. Best option would be in the editor, then the build pipeline, the resource loading in the engine, then the Lua interface. After that, the resource should either be ok, or the owner should handle it gracefully.

Another example is setting memory to zero, just before explicitly setting the members one by one.
In such a case, the memset isn't needed.

Avoiding patterns like this keeps code size to a minimum.

## Optimizations

When people mention optimizations, they can mean different things, and often they refer to a specific stage at the end of a project.
We want performance to be there from day one, making sure each project can be developed as quickly as possible.

We always try to think of the use cases and the expected performance in the design phase of a feature.
We are not shy to implement features ourselves, in order to reach those goals.

Also, when doing optimizations do measure them, and attach the info to the pull request, so the reviewers can make a proper assesment of the fix.
E.g. compile time, runtime, memory etc.


## Third party libraries

Sometimes we need to use a third party library.
It is fine, if the cost of using it (code size, complexity, performance) is properly assessed.
Also make sure to list alternatives (such as implementing the feature yourself) in the design phase.

## Debug vs Release

We don't really distinguish between the traditional Debug or Release concept.
We compile (and ship) all our libraries with the `-O2` flag.

Instead, we compartmentalize functionality into separate libraries, and during link time, either add `featureX_impl.a` or a `featureX_null.a` to the command line.

If you _do_ need to build local libraries for better denug info, use the `--opt-level=0` flag on the command line.
You can also use the `./scripts/submodule.sh` to rebuild a single library with `O0` and then relink the engine (usually the fastest option).

## Defines

We do not use any release/debug defines, but rather compiler specific defines to control certain behavior when required.
Most often, this occurs in the lower layers of the engine, such as `dlib`.

## Platform differences

For larger differences, we put them into separate files. E.g. `file_win32.cpp` vs `file_posix.cpp`.
For small differences, we use compiler defines directly in a function.

```c++
int SomeFunction(int value)
{
#if defined(WIN32)
    return 0;
#else
    return value ? 1 : 0
#endif
}
```

#### Http Cache

Assets loaded with dmResource are cached locally. A non-standard batch-oriented cache validation mechanism used if available in order to speed up the cache-validation process. See dlib, *dmHttpCache* and *ConsistencyPolicy*, for more information.


### Engine Extensions

Script extensions can be created using a simple extensions mechanism. To add a new extension to the engine the only required step is to link with the extension library and set "exported_symbols" in the wscript, see note below.

We also use the same mechanic for our [native extensions](./NATIVE_EXTENSIONS.md).

### Porting to another compiler

You will likely need to recompile external libraries. Check the `share/ext` folder for building the external packages for each platform (Source code for some old packages is available [here](https://drive.google.com/open?id=0BxFxQdv6jzseeXh2TzBnYnpwdUU).)


## API style

We favor C-style api's that create a context, given some parameters.
This context can then be operated on, and eventually destroyed:

```c
typedef struct SystemContext; // opaque type to keep implementation private
SystemContext* ctx = SystemCreate(...);
SystemUpdate(ctx, ...);
SystemDestroy(ctx);
```

```c++
// Similarly, but in C++
dmSystem::Context* ctx = dmSystem::Create(...);
dmSystem::Update(ctx, ...);
dmSystem::Destroy(ctx);
```

## API documentation

API documentation is generated from source comments. See [README_DOCS.md](../../README_DOCS.md) for help on how to document your api's.

### Singletons

We try to stay away from singletons, but you may still find them occasionally when the code flow hasn't been updated enough to support a context-based approach.

