package com.dynamo.bob.bundle.test;

import static org.junit.Assert.assertEquals;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.text.ParseException;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

import org.eclipse.core.runtime.Platform;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.bob.OsgiResourceScanner;
import com.dynamo.bob.bundle.BundleHelper;
import com.dynamo.bob.bundle.BundleHelper.ResourceInfo;
import com.dynamo.bob.fs.ClassLoaderMountPoint;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.util.BobProjectProperties;

public class BundleHelperTest {

    ClassLoaderMountPoint mp;

    @Before
    public void setUp() throws Exception {
        this.mp = new ClassLoaderMountPoint(null, "com/dynamo/bob/bundle/test/*", new OsgiResourceScanner(Platform.getBundle("com.dynamo.cr.bob")));
        this.mp.mount();
    }

    @After
    public void tearDown() throws Exception {
        this.mp.unmount();
    }

    void testValue(String p, String c, String k, Object expected) throws IOException, ParseException {
        BobProjectProperties properties = new BobProjectProperties();
        properties.load(new ByteArrayInputStream(p.getBytes()));
        Map<String, Map<String, Object>> map = BundleHelper.createPropertiesMap(properties);

        Object actual = map.get(c).get(k);
        assertEquals(expected, actual);
    }

    @Test
    public void testProperties() throws IOException, ParseException {
        testValue("[project]\nwrite_log=0", "project", "write_log", false);
        testValue("[project]\nwrite_log=1", "project", "write_log", true);
        testValue("", "project", "write_log", false);
        testValue("", "project", "title", "unnamed");
    }

    // Thx, Java
    public static boolean compareStr(String str1, String str2) {
        return (str1 == null ? str2 == null : str1.equals(str2));
    }

    static protected boolean checkIssue(List<ResourceInfo> issues, String resource, int lineNumber, String severity, String message) {
        for ( ResourceInfo info : issues) {
            if (compareStr(info.resource, resource) && info.lineNumber == lineNumber && info.severity.equals(severity) && info.message.equals(message)) {
                return true;
            }
        }
        return false;
    }

    static void debugIssues(List<ResourceInfo> issues) {
        for (ResourceInfo info : issues) {
            System.out.println(String.format("ISSUE: %s : %d: %s - \"%s\"", info.resource, info.lineNumber, info.severity, info.message));
        }
    }

    @Test
    public void testErrorLog() throws IOException {

        {
            IResource resource = this.mp.get("com/dynamo/bob/bundle/test/errorLogAndroid.txt");
            String log = new String(resource.getContent());
            List<ResourceInfo> issues = new ArrayList<ResourceInfo>();
            BundleHelper.parseLog("armv7-android", log, issues);

            assertEquals(true, checkIssue(issues, "androidnative/src/main.cpp", 37, "error", "In constructor '{anonymous}::AttachScope::AttachScope()':\n'Attach' was not declared in this scope\n     AttachScope() : m_Env(Attach())"));
            assertEquals(false, checkIssue(issues, null, 0, "warning", "[options] bootstrap class path not set in conjunction with -source 1.6"));
            assertEquals(true, checkIssue(issues, "androidnative/src/main.cpp", 17, "error", "'ubar' does not name a type\n ubar g_foo = 0;"));
            assertEquals(true, checkIssue(issues, "androidnative/src/main.cpp", 147, "error", "undefined reference to 'Foobar()'\ncollect2: error: ld returned 1 exit status"));
        }
        {
            IResource resource = this.mp.get("com/dynamo/bob/bundle/test/errorLogWin32.txt");
            String log = new String(resource.getContent());
            List<ResourceInfo> issues = new ArrayList<ResourceInfo>();
            BundleHelper.parseLog("x86_64-win32", log, issues);

            assertEquals(true, checkIssue(issues, "androidnative/src/main.cpp", 17, "error", "missing type specifier - int assumed. Note: C++ does not support default-int"));
            assertEquals(true, checkIssue(issues, "androidnative/src/main.cpp", 17, "error", "syntax error: missing ';' before identifier 'g_foo'"));
            assertEquals(true, checkIssue(issues, "main.cpp_0.o", 1, "error", "unresolved external symbol \"void __cdecl Foobar(void)\" (?Foobar@@YAXXZ) referenced in function \"enum dmExtension::Result __cdecl AppInitializeExtension(struct dmExtension::AppParams * __ptr64)\" (?AppInitializeExtension@@YA?AW4Result@dmExtension@@PEAUAppParams@2@@Z)"));
        }
        {
            IResource resource = this.mp.get("com/dynamo/bob/bundle/test/errorLogOSX.txt");
            String log = new String(resource.getContent());
            List<ResourceInfo> issues = new ArrayList<ResourceInfo>();
            BundleHelper.parseLog("x86_64-osx", log, issues);

            assertEquals(true, checkIssue(issues, "androidnative/src/main.cpp", 15, "error", "use of undeclared identifier 'Hello'\n    Hello();"));
            assertEquals(true, checkIssue(issues, "androidnative/src/main.cpp", 17, "error", "unknown type name 'ubar'\nubar g_foo = 0;"));
            assertEquals(true, checkIssue(issues, null, 1, "error", "Undefined symbols for architecture x86_64:\n  \"__Z6Foobarv\", referenced from:\n      __ZL22AppInitializeExtensionPN11dmExtension9AppParamsE in libb5622585-1c2d-4455-9d8d-c29f9404a475.a(main.cpp_0.o)"));
        }
        {
            IResource resource = this.mp.get("com/dynamo/bob/bundle/test/errorLogiOS.txt");
            String log = new String(resource.getContent());
            List<ResourceInfo> issues = new ArrayList<ResourceInfo>();
            BundleHelper.parseLog("arm64-ios", log, issues);

            assertEquals(true, checkIssue(issues, "androidnative/src/main.cpp", 17, "error", "unknown type name 'ubar'\nubar g_foo = 0;"));
            assertEquals(true, checkIssue(issues, null, 1, "error", "Undefined symbols for architecture arm64:\n  \"__Z6Foobarv\", referenced from:\n      __ZL22AppInitializeExtensionPN11dmExtension9AppParamsE in lib44391c30-91a4-4faf-aef6-2dbc429af9ed.a(main.cpp_0.o)"));
        }

        {
            IResource resource = this.mp.get("com/dynamo/bob/bundle/test/errorLogHTML5.txt");
            String log = new String(resource.getContent());
            List<ResourceInfo> issues = new ArrayList<ResourceInfo>();
            BundleHelper.parseLog("js-web", log, issues);

            assertEquals(true, checkIssue(issues, "androidnative/src/main.cpp", 17, "error", "unknown type name 'ubar'\nubar g_foo = 0;"));
        }

        {
            IResource resource = this.mp.get("com/dynamo/bob/bundle/test/errorLogLinux.txt");
            String log = new String(resource.getContent());
            List<ResourceInfo> issues = new ArrayList<ResourceInfo>();
            BundleHelper.parseLog("x86_64-linux", log, issues);

            assertEquals(true, checkIssue(issues, "androidnative/src/main.cpp", 17, "error", "‘ubar’ does not name a type\n ubar g_foo = 0;"));
            assertEquals(true, checkIssue(issues, "androidnative/src/main.cpp", 166, "error", "undefined reference to `Foobar()'"));
        }
    }
}
