// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

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
import java.util.HashMap;
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
import com.dynamo.bob.fs.IFileSystem;
import com.dynamo.bob.util.LibraryUtil;
import com.dynamo.bob.test.util.MockFileSystem;

public class ProjectTest {

    private class MockProject extends Project {
        public HashMap<String,String> env;

        public MockProject(IFileSystem fileSystem, String sourceRootDirectory, String buildDirectory) {
            super(fileSystem, sourceRootDirectory, buildDirectory);
            env = new HashMap<>();
        }

        @Override
        public String getSystemEnv(String name) {
            return env.get(name);
        }
    }

    private final static int SERVER_PORT = 8081;
    private final static String EMAIL = "unittest@defold.com";
    private final static String AUTH = "secret-auth";
    private final static String BASIC_AUTH = "user:secret";
    private final static String BASIC_AUTH_ENV_TOKEN = "user:__TOKEN__";
    private final static String BASIC_AUTH_ENV_TOKEN_RESOLVED = "user:resolved";

    private MockFileSystem fileSystem;
    private MockProject project;
    private Server httpServer;
    private ArrayList<URL> libraryUrls = new ArrayList<URL>();

    private AtomicInteger _304Count = new AtomicInteger();

    @Rule
    public TestLibrariesRule testLibs = new TestLibrariesRule();

    private void initHttpServer(String serverLocation) throws IOException {
        System.out.printf("initHttpServer start");
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
        System.out.printf("initHttpServer end");
    }

    @Before
    public void setUp() throws Exception {
        System.out.printf("setUp start");
        // See TestLibrariesRule.java for the creation of these zip files
        libraryUrls = new ArrayList<URL>();
        libraryUrls.add(new URL("http://localhost:8081/test_lib1.zip"));
        libraryUrls.add(new URL("http://localhost:8081/test_lib2.zip"));
        libraryUrls.add(new URL("http://" + BASIC_AUTH + "@localhost:8081/test_lib5.zip"));
        libraryUrls.add(new URL("http://" + BASIC_AUTH_ENV_TOKEN + "@localhost:8081/test_lib6.zip"));
        libraryUrls.add(new URL("http://localhost:8081/test.zip"));

        fileSystem = new MockFileSystem();
        project = new MockProject(fileSystem, Files.createTempDirectory("defold_").toString(), "build/default");
        project.env.put("TOKEN", "resolved");
        project.setOption("email", EMAIL);
        project.setOption("auth", AUTH);
        project.scan(new ClassLoaderScanner(), "com.dynamo.bob.test");
        project.setLibUrls(libraryUrls);

        initHttpServer(testLibs.getServerLocation());
        System.out.printf("setUp end");
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
        System.out.printf("testResolve begin");

        assertEquals(0, _304Count.get());
        File libDir = new File(project.getLibPath());
        if (libDir.exists()) {
            FileUtils.cleanDirectory(libDir);
        }

        this.project.resolveLibUrls(new NullProgress());

        File currentFiles[] = libDir.listFiles(File::isFile);

        for (URL url : libraryUrls) {
            String hashedUrl = LibraryUtil.getHashedUrl(url);
            boolean found = false;
            for (File f : currentFiles) {
                if (LibraryUtil.matchUri(hashedUrl, f.getName())) {
                    found = true;
                    break;
                }
            }
            assertTrue(found);
        }

        assertEquals(0, _304Count.get());

        this.project.resolveLibUrls(new NullProgress());

        currentFiles = libDir.listFiles(File::isFile);
        List<File> filenames = new ArrayList<>();
        for (URL url : libraryUrls) {
            String hashedUrl = LibraryUtil.getHashedUrl(url);
            boolean found = false;
            for (File f : currentFiles) {
                if (LibraryUtil.matchUri(hashedUrl, f.getName())) {
                    found = true;
                    filenames.add(f);
                    break;
                }
            }
            assertTrue(found);
        }
        assertEquals(filenames.size(), _304Count.get());

        System.out.printf("testResolve end");
    }

    @Test
    public void testMountPoints() throws Exception {
        System.out.printf("testMountPoints start");
        project.resolveLibUrls(new NullProgress());
        project.mount(new ClassLoaderResourceScanner());
        project.setInputs(Arrays.asList("test_lib1/file1.in", "test_lib2/file2.in", "test_lib5/file5.in", "builtins/cp_test.in"));
        List<TaskResult> results = build("resolve", "build");
        assertEquals(4, results.size());
        for (TaskResult result : results) {
            assertTrue(result.isOk());
        }
        System.out.printf("end");
    }

    @Test
    public void testMountPointFindSources() throws Exception {
        System.out.printf("testMountPointFindSources start");
        project.resolveLibUrls(new NullProgress());
        project.mount(new ClassLoaderResourceScanner());
        project.setInputs(Arrays.asList("test_lib2/file2.in", "test_lib1/file1.in", "test_lib6/file6.in", "test_lib5/file5.in"));
        List<TaskResult> results = build("build");
        assertFalse(results.isEmpty());
        for (TaskResult result : results) {
            assertTrue(result.isOk());
        }

        System.out.printf("end");
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
        System.out.printf("testFindResourcePaths start");
        libraryUrls.add(new URL("http://localhost:8081/test_lib3.zip"));
        project.resolveLibUrls(new NullProgress());
        project.mount(new ClassLoaderResourceScanner());
        project.setInputs(Arrays.asList("test_lib1/file1.in", "test_lib2/file2.in", "test_lib1/subdir/file5.in", "builtins/cp_test.in"));

        List<String> results = new ArrayList<String>();
        project.findResourcePaths(".", results);
        results = filterBuiltins(results);

        assertFalse(results.isEmpty());
        assertEquals(7, results.size());
        System.out.printf("end");
    }

    @Test
    public void testFindResourceDirs() throws Exception {
        System.out.printf("testFindResourceDirs start");
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
        System.out.printf("end");
    }

    @Test
    public void testAllResourcePathsCacheLibrary() throws Exception {
        project.resolveLibUrls(new NullProgress());
        project.mount(new ClassLoaderResourceScanner());
        project.setInputs(Arrays.asList("test/file.in", "builtins/cp_test.in"));
        List<TaskResult> results = build("resolve", "build");
        assertEquals(2, results.size());
        for (TaskResult result : results) {
            assertTrue(result.isOk());
        }

        ArrayList<String> pathResults = new ArrayList<String>();
        project.findResourcePaths("/test_non", pathResults);
        assertEquals(0, pathResults.size());

        ArrayList<String> pathResults1 = new ArrayList<String>();
        project.findResourcePaths("/test/file.in", pathResults1);
        assertEquals(1, pathResults1.size());

        ArrayList<String> pathResults2 = new ArrayList<String>();
        project.findResourcePaths("/test", pathResults2);
        assertEquals(1, pathResults2.size());
    }

    private class FileHandler extends ResourceHandler {
        public void handle(String target, Request baseRequest, HttpServletRequest request, HttpServletResponse response) throws IOException ,javax.servlet.ServletException {

            // Verify auth
            boolean authenticated = false;
            if (request.getHeader("Authorization") != null) {
                // Basic auth should also not send X-Email or X-Auth
                String decomposedAuthString1 = "Basic " + new String(new Base64().encode(BASIC_AUTH.getBytes()));
                String decomposedAuthString2 = "Basic " + new String(new Base64().encode(BASIC_AUTH_ENV_TOKEN_RESOLVED.getBytes()));
                boolean equals1 = decomposedAuthString1.equals(request.getHeader("Authorization"));
                boolean equals2 = decomposedAuthString2.equals(request.getHeader("Authorization"));
                authenticated = ((equals1 || equals2) && request.getHeader("X-Email") == null && request.getHeader("X-Auth") == null);
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

                try {

                    String etag = request.getHeader("If-None-Match");
                    if (sha1 != null) {
                        response.setHeader("ETag", String.format("\"%s\"", sha1));
                    } else {
                        sha1 = "";
                    }

                    if (etag != null && etag.equals(String.format("\"%s\"", sha1))) {
                        _304Count.incrementAndGet();
                        response.setStatus(304);
                        baseRequest.setHandled(true);
                    } else {
                        super.handle(target, baseRequest, request, response);
                    }
                } catch (Exception e) {
                    e.printStackTrace();
                    throw e;
                }

            } else {
                response.setStatus(HttpServletResponse.SC_UNAUTHORIZED);
            }
        }
    }
}

