# PKIX path building failed. Unable to find valid certification path to requested target.

`javax.net.ssl.SSLHandshakeException: sun.security.validator.ValidatorException: PKIX path building failed: sun.security.provider.certpath.SunCertPathBuilderException: unable to find valid certification path to requested target`

This exception occurs when the editor tries to make an https connection but the certificate chain provided by the server cannot be verified.

The Java Runtime Environment we bundle with the editor comes with a list of trusted CA certificates, found in ``packages/jdk-25+36/lib/security/cacerts``. You can inspect and modify this list using [keytool](https://docs.oracle.com/en/java/javase/21/docs/specs/man/keytool.html). The default password is `changeit`.

```
$ keytool -list -v -keystore packages/jdk-25+36/lib/security/cacerts -storepass changeit
```

## Common causes

### An intermediate CA certificate is newer than the JRE we bundle

A workaround is to import the missing certificate using `keytool -importcert ...`.

### Antivirus software is scanning https traffic

Avast, Kaspersky and others have an option to scan https traffic. This is accomplished by intercepting/proxying connections. In effect the editor connects to the antivirus software (thinking its the target server), and the antivirus software connects to the actual target server. Similar to a man-in-the-middle attack.

The certificate chain seen by the editor will contain a certificate issued by the antivirus software, and this is typically not in the `cacerts` file so the connection will be refused.

To workaround this problem you can either import the certificate, or disable scanning of encrypted network traffic in your antivirus software. We might add a feature in the editor to more easily add additional trusted certificates.

More information:

* [Avast](https://blog.avast.com/2015/05/25/explaining-avasts-https-scanning-feature/)
* [Kaspersky](https://support.kaspersky.com/6851)

## Debugging

To help debug ssl issues, you can start the editor manually with an additional `-Djavax.net.debug=SSL` flag.

### Windows

Start `cmd.exe` and `cd` into the Defold directory. Then run:

```
packages\jdk-25+36\bin\java -Djna.nosys=true -Ddefold.launcherpath=. -Ddefold.resourcespath=. -Ddefold.version=12345 -Ddefold.editor.sha1=<editor_sha1 from config file> -Ddefold.engine.sha1=testing -Ddefold.buildtime=testing -Ddefold.channel= -Djava.ext.dirs=packages\jre\lib\ext -Djava.net.preferIPv4Stack=true -Dsun.net.client.defaultConnectTimeout=30000 -Dsun.net.client.defaultReadTimeout=30000 -Djogl.texture.notexrect=true -Dglass.accessible.force=false -Djavax.net.debug=SSL -jar packages\defold-<editor_sha1 from config file>.jar
```

### Mac

Open a terminal window and `cd` into the `Defold.app/Contents/Resources` directory. Then run:

```
packages/jdk-25+36/bin/java -Djna.nosys=true -Ddefold.launcherpath=. -Ddefold.resourcespath=. -Ddefold.version=12345 -Ddefold.editor.sha1=<editor_sha1 from config file> -Ddefold.engine.sha1=testing -Ddefold.buildtime=testing -Ddefold.channel= -Djava.ext.dirs=packages/jre/lib/ext -Djava.net.preferIPv4Stack=true -Dsun.net.client.defaultConnectTimeout=30000 -Dsun.net.client.defaultReadTimeout=30000 -Djogl.texture.notexrect=true -Dglass.accessible.force=false -Djavax.net.debug=SSL -jar packages/defold-<editor_sha1 from config file>.jar
```

### Linux

Open a terminal window and `cd` into the Defold directory. Then run:

```
packages/jdk-25+36/bin/java -Djna.nosys=true -Ddefold.launcherpath=. -Ddefold.resourcespath=. -Ddefold.version=12345 -Ddefold.editor.sha1=<editor_sha1 from config file> -Ddefold.engine.sha1=testing -Ddefold.buildtime=testing -Ddefold.channel= -Djava.ext.dirs=packages/jre/lib/ext -Djava.net.preferIPv4Stack=true -Dsun.net.client.defaultConnectTimeout=30000 -Dsun.net.client.defaultReadTimeout=30000 -Djogl.texture.notexrect=true -Dglass.accessible.force=false -Djavax.net.debug=SSL -jar packages/defold-<editor_sha1 from config file>.jar
```
