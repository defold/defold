package com.dynamo.bob.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.io.File;
import java.io.IOException;
import java.net.URL;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.zip.ZipFile;

import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.apache.commons.codec.binary.Base64;

import org.eclipse.jetty.server.Request;
import org.eclipse.jetty.server.Server;
import org.eclipse.jetty.server.bio.SocketConnector;
import org.eclipse.jetty.server.handler.HandlerList;
import org.eclipse.jetty.server.handler.ResourceHandler;
import org.eclipse.jetty.util.resource.Resource;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.ClassLoaderScanner;
import com.dynamo.bob.ClassLoaderResourceScanner;
import com.dynamo.bob.MultipleCompileException;
import com.dynamo.bob.NullProgress;
import com.dynamo.bob.Project;
import com.dynamo.bob.TaskResult;
import com.dynamo.bob.util.LibraryUtil;
import com.dynamo.bob.test.util.MockFileSystem;

public class ProjectTest {

    private final static int SERVER_PORT = 8081;
    private final static String EMAIL = "unittest@defold.com";
    private final static String AUTH = "secret-auth";
    private final static String BASIC_AUTH = "user:secret";

    private MockFileSystem fileSystem;
    private Project project;
    //private Bundle bundle;
    private Server httpServer;
    private ArrayList<URL> libraryUrls = new ArrayList<URL>();

    private AtomicInteger _304Count = new AtomicInteger();

    @Rule
    public TestLibrariesRule testLibs = new TestLibrariesRule();

    private void initHttpServer(String serverLocation) throws IOException {
        httpServer = new Server();

        SocketConnector connector = new SocketConnector();
        connector.setPort(SERVER_PORT);
        httpServer.addConnector(connector);
        HandlerList handlerList = new HandlerList();
        FileHandler fileHandler = new FileHandler();
        fileHandler.setResourceBase(serverLocation);
        handlerList.addHandler(fileHandler);
        httpServer.setHandler(handlerList);

        try {
            httpServer.start();
        } catch (Exception e) {
            throw new IOException("Unable to start http server", e);
        }
    }

    @Before
    public void setUp() throws Exception {
        // See TestLibrariesRule.java for the creation of these zip files
        libraryUrls = new ArrayList<URL>();
        libraryUrls.add(new URL("http://localhost:8081/test_lib1.zip"));
        libraryUrls.add(new URL("http://localhost:8081/test_lib2.zip"));
        libraryUrls.add(new URL("http://" + BASIC_AUTH + "@localhost:8081/test_lib5.zip"));

        //bundle = Platform.getBundle("com.dynamo.cr.bob");
        fileSystem = new MockFileSystem();
        project = new Project(fileSystem, Files.createTempDirectory("defold_").toString(), "build/default");
        project.setOption("email", EMAIL);
        project.setOption("auth", AUTH);
        project.scan(new ClassLoaderScanner(), "com.dynamo.bob.test");
        project.setLibUrls(libraryUrls);

        initHttpServer(testLibs.getServerLocation());
    }

    @After
    public void tearDown() throws Exception {
        httpServer.stop();
        project.dispose();
    }

    List<TaskResult> build(String... commands) throws IOException, CompileExceptionError, MultipleCompileException {
        return project.build(new NullProgress(), commands);
    }

    private boolean libExists(String lib) {
        return new File(FilenameUtils.concat(project.getLibPath(), lib)).exists();
    }

    @Test
    public void testResolve() throws Exception {
        assertEquals(0, _304Count.get());
        File lib = new File(project.getLibPath());
        if (lib.exists()) {
            FileUtils.cleanDirectory(new File(project.getLibPath()));
        }

        ArrayList<String> filenames = new ArrayList<String>();
        for (URL url : libraryUrls) {
            filenames.add(LibraryUtil.libUrlToFilename(url));
        }

        this.project.resolveLibUrls(new NullProgress());
        for (String filename : filenames) {
            assertTrue(libExists(filename));
        }
        assertEquals(0, _304Count.get());

        this.project.resolveLibUrls(new NullProgress());
        for (String filename : filenames) {
            assertTrue(libExists(filename));
        }
        assertEquals(filenames.size(), _304Count.get());
    }

    @Test
    public void testMountPoints() throws Exception {
        project.resolveLibUrls(new NullProgress());
        project.mount(new ClassLoaderResourceScanner());
        project.setInputs(Arrays.asList("test_lib1/file1.in", "test_lib2/file2.in", "test_lib5/file5.in", "builtins/cp_test.in"));
        List<TaskResult> results = build("resolve", "build");
        assertEquals(4, results.size());
        for (TaskResult result : results) {
            assertTrue(result.isOk());
        }
    }

    @Test
    public void testMountPointFindSources() throws Exception {
        project.resolveLibUrls(new NullProgress());
        project.mount(new ClassLoaderResourceScanner());
        project.findSources(".", null);
        List<TaskResult> results = build("build");
        assertFalse(results.isEmpty());
        for (TaskResult result : results) {
            assertTrue(result.isOk());
        }
    }

    // due to bob.jar including builtins/ we get way too many resources
    // The only test builtin, is the 'cp_test.in'
    private List<String> filterBuiltins(List<String> allResults) {
        List<String> results = new ArrayList<String>();
        for (String resource : allResults) {
            if (resource.startsWith("builtins") && !resource.endsWith("cp_test.in")) {
                continue;
            }
            results.add(resource);
        }
        return results;
    }


    @Test
    public void testFindResourcePaths() throws Exception {
        libraryUrls.add(new URL("http://localhost:8081/test_lib3.zip"));
        project.resolveLibUrls(new NullProgress());
        project.mount(new ClassLoaderResourceScanner());
        project.setInputs(Arrays.asList("test_lib1/file1.in", "test_lib2/file2.in", "test_lib1/subdir/file5.in", "builtins/cp_test.in"));

        List<String> results = new ArrayList<String>();
        project.findResourcePaths(".", results);
        results = filterBuiltins(results);

        assertFalse(results.isEmpty());
        assertEquals(5, results.size());
    }

    @Test
    public void testFindResourceDirs() throws Exception {
        libraryUrls.add(new URL("http://localhost:8081/test_lib3.zip"));
        project.resolveLibUrls(new NullProgress());
        project.mount(new ClassLoaderResourceScanner());
        project.setInputs(Arrays.asList("test_lib1/file1.in", "test_lib2/file2.in", "test_lib1/subdir/file5.in", "builtins/cp_test.in"));

        List<String> results = new ArrayList<String>();
        project.findResourceDirs("", results);

        results = filterBuiltins(results);

        assertTrue(results.contains("test_lib1"));
        assertTrue(results.contains("test_lib2"));
        assertTrue(results.contains("test_lib3"));
        assertTrue(results.contains("test_lib5"));

        results = new ArrayList<String>();
        project.findResourceDirs("test_lib1/", results);
        assertEquals(2, results.size());
        assertTrue(results.contains("testdir1"));
        assertTrue(results.contains("testdir2"));
    }

    private class FileHandler extends ResourceHandler {
        public void handle(String target, Request baseRequest, HttpServletRequest request, HttpServletResponse response) throws IOException ,javax.servlet.ServletException {

            // Verify auth
            boolean authenticated = false;
            if (request.getHeader("Authorization") != null) {
                // Basic auth should also not send X-Email or X-Auth
                String decomposedAuthString = "Basic " + new String(new Base64().encode(BASIC_AUTH.getBytes()));
                authenticated = decomposedAuthString.equals(request.getHeader("Authorization")) && request.getHeader("X-Email") == null && request.getHeader("X-Auth") == null;
            } else {
                authenticated = EMAIL.equals(request.getHeader("X-Email")) && AUTH.equals(request.getHeader("X-Auth"));
            }

            if (authenticated) {

                String sha1 = null;
                Resource resource = getResource(request);
                File file = resource.getFile();
                if (file.exists()) {
                    ZipFile zip = new ZipFile(file);
                    sha1 = zip.getComment();
                    zip.close();
                }

                String etag = request.getHeader("If-None-Match");
                if (sha1 != null) {
                    response.setHeader("ETag", sha1);
                }
                if (etag != null && etag.equals(sha1)) {
                    _304Count.incrementAndGet();
                    response.setStatus(304);
                    baseRequest.setHandled(true);
                } else {
                    super.handle(target, baseRequest, request, response);
                }

            } else {
                response.setStatus(HttpServletResponse.SC_UNAUTHORIZED);
            }
        }
    }
}

