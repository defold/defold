package com.dynamo.bob.test;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.io.File;
import java.io.IOException;
import java.net.URL;
import java.util.Arrays;
import java.util.List;

import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.eclipse.core.runtime.FileLocator;
import org.eclipse.core.runtime.Platform;
import org.eclipse.jetty.server.Request;
import org.eclipse.jetty.server.Server;
import org.eclipse.jetty.server.bio.SocketConnector;
import org.eclipse.jetty.server.handler.HandlerList;
import org.eclipse.jetty.server.handler.ResourceHandler;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.osgi.framework.Bundle;

import com.dynamo.bob.ClassLoaderResourceScanner;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.NullProgress;
import com.dynamo.bob.OsgiResourceScanner;
import com.dynamo.bob.OsgiScanner;
import com.dynamo.bob.Project;
import com.dynamo.bob.test.util.MockFileSystem;
import com.dynamo.bob.TaskResult;

public class ProjectTest {
    
    private final static int SERVER_PORT = 8081;
    private final static String EMAIL = "test@king.com";
    private final static String AUTH = "secret-auth";

    private MockFileSystem fileSystem;
    private Project project;
    private Bundle bundle;
    private Server httpServer;

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
        bundle = Platform.getBundle("com.dynamo.cr.bob");
        fileSystem = new MockFileSystem();
        project = new Project(fileSystem, FileUtils.getTempDirectoryPath(), "build/default");
        project.setOption("email", EMAIL);
        project.setOption("auth", AUTH);
        project.scan(new OsgiScanner(bundle), "com.dynamo.bob.test");
        project.setLibUrls(Arrays.asList(new URL("http://localhost:8081/test_lib.zip"), new URL("http://localhost:8081/test_lib2.zip")));
        String serverLocation = FileLocator.resolve(getClass().getClassLoader().getResource("server_root")).getPath();
        initHttpServer(serverLocation);
    }

    @After
    public void tearDown() throws Exception {
        httpServer.stop();
        project.dispose();
    }

    List<TaskResult> build(String... commands) throws IOException, CompileExceptionError {
        return project.build(new NullProgress(), commands);
    }

    private boolean libExists(String lib) {
        return new File(FilenameUtils.concat(project.getLibPath(), lib)).exists();
    }

    @Test
    public void testResolve() throws Exception {
        File lib = new File(project.getLibPath());
        if (lib.exists()) {
            FileUtils.cleanDirectory(new File(project.getLibPath()));
        }
        assertFalse(libExists("test_lib.zip"));
        assertFalse(libExists("test_lib2.zip"));
        this.project.resolveLibUrls();
        assertTrue(libExists("test_lib.zip"));
        assertTrue(libExists("test_lib2.zip"));
    }

    @Test
    public void testMountPoints() throws Exception {
        project.resolveLibUrls();
        project.mount(null);
        project.setInputs(Arrays.asList("test_lib/file1.in", "test_lib2/file2.in", "builtins/cp_test.in"));
        List<TaskResult> results = build("resolve", "build");
        assertFalse(results.isEmpty());
        for (TaskResult result : results) {
            assertTrue(result.isOk());
        }
    }

    @Test
    public void testMountPointFindSources() throws Exception {
        project.resolveLibUrls();
        project.mount(new OsgiResourceScanner(Platform.getBundle("com.dynamo.cr.bob")));
        project.findSources(".", null);
        List<TaskResult> results = build("build");
        assertFalse(results.isEmpty());
        for (TaskResult result : results) {
            assertTrue(result.isOk());
        }
    }

    private class FileHandler extends ResourceHandler {
        public void handle(String target, Request baseRequest, HttpServletRequest request, HttpServletResponse response) throws IOException ,javax.servlet.ServletException {
            if (EMAIL.equals(request.getHeader("X-Email")) && AUTH.equals(request.getHeader("X-Auth"))) {
                super.handle(target, baseRequest, request, response);
            } else {
                response.setStatus(HttpServletResponse.SC_UNAUTHORIZED);
            }
        }
    }
}

