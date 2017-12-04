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

    @Test
    public void testErrorLog() throws IOException {
        IResource resource = this.mp.get("com/dynamo/bob/bundle/test/errorLogExamples.txt");
        String log = new String(resource.getContent());

        List<ResourceInfo> issues = new ArrayList<ResourceInfo>();
        BundleHelper.parseLog(log, issues);

        assertEquals(true, checkIssue(issues, "androidnative/src/main.cpp", 37, "error", "In constructor '{anonymous}::AttachScope::AttachScope()':\n'Attach' was not declared in this scope\n     AttachScope() : m_Env(Attach())"));
        assertEquals(false, checkIssue(issues, null, 0, "warning", "[options] bootstrap class path not set in conjunction with -source 1.6"));
        assertEquals(true, checkIssue(issues, "androidnative/src/main.cpp", 11, "error", "'sdfsdfsdf' does not name a type\n sdfsdfsdf "));
    }
}
