package com.dynamo.bob.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.junit.matchers.JUnitMatchers.hasItem;

import java.io.File;
import java.io.IOException;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;
import org.apache.commons.lang3.StringUtils;
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

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.NullProgress;
import com.dynamo.bob.OsgiScanner;
import com.dynamo.bob.Project;
import com.dynamo.bob.test.util.MockFileSystem;
import com.dynamo.bob.TaskResult;

public class ProjectTest {
    
    private final static int SERVER_PORT = 8080;
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
        project = new Project(fileSystem, FileUtils.getTempDirectory().getAbsolutePath(), "build/default");
        project.setOption("email", EMAIL);
        project.setOption("auth", AUTH);
        project.scan(new OsgiScanner(bundle), "com.dynamo.bob.test");
        fileSystem.addFile("/game.project", IOUtils.toByteArray(getClass().getResourceAsStream("game.project")));
        String serverLocation = FileLocator.resolve(getClass().getClassLoader().getResource("server_root")).getPath();
        initHttpServer(serverLocation);
    }

    @After
    public void tearDown() throws Exception {
        httpServer.stop();
    }

    List<TaskResult> resolve() throws IOException, CompileExceptionError {
        return project.build(new NullProgress(), "resolve");
    }

    @Test
    public void testResolve() throws Exception {
        File testLib = new File(FilenameUtils.concat(project.getLibPath(), "test_lib.zip"));
        if (testLib.exists()) {
            testLib.delete();
        }
        resolve();
        assertTrue(testLib.exists());
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

