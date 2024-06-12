# glGenBuffers

`com.jogamp.opengl.GLException: The graphics device does not support: glGenBuffers`

If you encounter this exception, first of all try updating your graphics drivers.
If that does not help, follow the instructions for your platform below.

Spotted an error? Found another solution? Open a PR! :)

## Mac / Linux

Open a new issue [here](https://github.com/defold/defold/issues) and attach the output from `glxinfo`

## Windows

To help you troubleshoot we need some info about your graphics card, for instance the output from [dxdiag](https://support.microsoft.com/en-us/help/4028644/windows-open-and-run-dxdiagexe) or a GL Report from [OpenGL ExtensionsViewer](www.realtech-vr.com/glview/). Open a new issue [here](https://github.com/defold/defold/issues) and attach the info.

### Windows 10 + older Intel HD integrated graphics card (HD2000, HD3000)

The editor runs on java, using jogamp for OpenGL support. Since Java 8u60, `java.exe` is marked Windows 10 Compliant by a small change in its embedded manifest. The Intel Windows 10 drivers only work on applications *not* marked Windows 10 Compliant. Go figure.

Read more about the situation here:

[jogamp thread](http://forum.jogamp.org/jogamp-using-Generic-GDI-driver-in-Windows-10-td4037477.html)

[java manifest change](http://hg.openjdk.java.net/jdk8u/jdk8u60/jdk/annotate/3d488a752d8d/src/windows/resource/java.manifest#l56)

[Bug report on intel drivers](https://software.intel.com/en-us/forums/graphics-driver-bug-reporting/topic/607695)

The editor appears to run fine if you undo the manifest change on the bundled `java.exe`. You can get a patched version here: [java.exe](https://github.com/defold/editor2-issues/raw/master/faq/java.exe)
You can do it yourself by following these instructions:

First you need the [Manifest Tool](https://msdn.microsoft.com/library/aa375649). It's distributed as part of the Windows SDK. You probably already have it if you have Visual Studio installed.


1. Make sure you have `mt.exe` installed
```
C:\Users\your.name>mt.exe
```
2. Move into the jre\bin subdirectory of your Defold installation
```
C:\Users\your.name>cd Defold\packages\jre\bin
```
3. Extract the manifest from `java.exe`
```
C:\Users\your.name\Defold\packages\jre\bin>mt -inputresource:java.exe -out:java.manifest
```
4. Open the `java.manifest` you extracted above in your favourite editor
```
C:\Users\your.name\Defold\packages\jre\bin>notepad java.manifest
```
5. Locate the compatibility section:
```
<compatibility xmlns="urn:schemas-microsoft-com:compatibility.v1">
  <application>
    <supportedOS Id="{e2011457-1546-43c5-a5fe-008deee3d3f0}"></supportedOS>
    <supportedOS Id="{35138b9a-5d96-4fbd-8e2d-a2440225f93a}"></supportedOS>
    <supportedOS Id="{4a2f28e3-53b9-4441-ba9c-d69d4a4a6e38}"></supportedOS>
    <supportedOS Id="{1f676c76-80e1-4239-95bb-83d0f6d0da78}"></supportedOS>
    <supportedOS Id="{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}"></supportedOS>
  </application>
</compatibility>
```
6. The last line `<supportedOS Id="{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}"></supportedOS>` is what marks `java.exe` Windows 10 Compliant. Remove it or comment it out. Save.
7. Embed the manifest again
```
C:\Users\your.name\Defold\packages\jre\bin>mt -manifest java.manifest -outputresource:java.exe;#1
```

8. Start the editor

### Windows under Parallels

Enable 3D acceleration under Hardware, Graphics, Advanced Settings.

### Windows under VirtualBox

Enable 3D acceleration under Display, Screen.
