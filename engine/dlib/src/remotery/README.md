
## Update the library

Copy the C files to engine:

    cp -v ~/notwork/Remotery/lib/*.* engine/dlib/src/remotery/


Copy the HTML files to the editor:

    cp -v -r ~/notwork/Remotery/vis/ editor/resources/engine-profiler/remotery/vis

## Manually update the code

Looking at the current code, we do a Defold fixup to modify the property name strings.
We do this in order to be able to have a prefix (`rmtp_`), but also to avoid displaying this prefix
when presented in the engine and in the HTML profile page.
