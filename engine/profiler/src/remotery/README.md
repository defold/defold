## Update Remotery

The engine sources (`Remotery.c`, `Remotery.h` and `RemoteryMetal.mm`) live in [lib](lib/). The editor serves the profiler from [editor/resources/engine-profiler/remotery/vis](../../../../editor/resources/engine-profiler/remotery/vis/).

When importing an upstream update, merge the corresponding library and viewer files into those directories. Compare the result with the checked-in versions to preserve Defold's changes, including the displayed property names (the `rmtp_` prefix is stripped), the viewer font rule in `Styles/Remotery.css`, and the editor resource paths in `patched.index.html`. Keep `index.html` usable as the standalone viewer.

The [old update script](update_remotery.sh) and [native patch](defold.patch) refer to the previous `engine/dlib` layout. They need updated paths before they can automate an import. The viewer changes are recorded in [defoldvis.patch](defoldvis.patch) and [issue-8146.patch](issue-8146.patch).

## Test the standalone viewer

From the repository root:

```sh
python3 -m http.server --bind 127.0.0.1 --directory editor/resources/engine-profiler/remotery/vis
```

Open `http://127.0.0.1:8000/` in a browser.
