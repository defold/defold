# Editor Product Build

Build configuration for editor and p2 repository.

## Building

Use build.py with command build_cr

## JRE bundling

JRE:s are downloaded from the s3-bucket defold-packages. The JRE:s at s3 are created
from the official Oracles JRE:s but repackaged to zip-files and with the root
directory renamed to *jre*. The JRE on OSX/Linux contains symbolic links that
must be preserved. The switch *-y* to zip preserves symblic links, e.g.


    # zip -y -r jre-8u5-macosx-x64 jre


## Notes

* Platform specific bundles must be annotated with os, ws and arch. Example:

        <plugin id="org.eclipse.swt.cocoa.macosx.x86_64" fragment="true"
                os="macosx" ws="cocoa" arch="x86_64"/>

  The annotation is an extension for Tycho and is not supported in Eclipse. Hence, the *Eclipse Product Configuration Editor*
  might remove such annotations.


  The following is added to cr.product. Otherwise the editor doesn't start.

     <configurations>
         <plugin id="org.eclipse.core.runtime" autoStart="true" startLevel="0" />
         <plugin id="org.eclipse.equinox.ds" autoStart="true" startLevel="2" />
     </configurations>

* To speed up then maven process when testing use the -rf option

        -rf :com.dynamo.cr.editor-product
  to limit the process to the com.dynamo.cr.editor-product module.
