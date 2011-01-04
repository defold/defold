package com.dynamo.cr.server.resources.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import java.io.File;
import java.io.IOException;
import java.net.URI;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

import javax.ws.rs.core.UriBuilder;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.client.ClientFactory;
import com.dynamo.cr.client.IBranchClient;
import com.dynamo.cr.client.IProjectClient;
import com.dynamo.cr.client.RepositoryException;
import com.dynamo.cr.common.providers.ProtobufProviders;
import com.dynamo.cr.proto.Config.ProjectConfiguration;
import com.dynamo.cr.protocol.proto.Protocol;
import com.dynamo.cr.protocol.proto.Protocol.BranchList;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus.Status;
import com.dynamo.cr.protocol.proto.Protocol.ResourceInfo;
import com.dynamo.cr.protocol.proto.Protocol.ResourceType;
import com.dynamo.cr.server.Server;
import com.dynamo.cr.server.util.FileUtil;
import com.dynamo.server.git.CommandUtil;
import com.sun.jersey.api.client.Client;
import com.sun.jersey.api.client.config.ClientConfig;
import com.sun.jersey.api.client.config.DefaultClientConfig;
import com.sun.jersey.api.client.filter.HTTPBasicAuthFilter;

public class ProjectResourceTest {

    private Server server;
    private IBranchClient branch_client;
    private IProjectClient project_client;

    int port = 6500;
    String user = "john";
    String passwd = "secret";
    private Client client;
    private ClientFactory factory;

    void execCommand(String command) throws IOException {
        CommandUtil.Result r = CommandUtil.execCommand(new String[] {"sh", command});
        if (r.exitValue != 0) {
            System.err.println(r.stdOut);
            System.err.println(r.stdErr);
        }
        assertEquals(0, r.exitValue);
    }

    @Before
    public void setUp() throws Exception {
        server = new Server("test_data/crepo_test.config");

        ClientConfig cc = new DefaultClientConfig();
        cc.getClasses().add(ProtobufProviders.ProtobufMessageBodyReader.class);
        cc.getClasses().add(ProtobufProviders.ProtobufMessageBodyWriter.class);

        client = Client.create(cc);
        client.addFilter(new HTTPBasicAuthFilter(user, passwd));

        URI uri;
        uri = UriBuilder.fromUri("http://localhost/proj1").port(port).build();
        factory = new ClientFactory(client);
        project_client = factory.getProjectClient(uri);

        branch_client = project_client.getBranchClient(user, "branch1");

        FileUtil.removeDir(new File(server.getBranchRoot()));

        execCommand("scripts/setup_testdata.sh");
    }

    @After
    public void tearDown() throws Exception {
        server.stop();
    }

    /*
     * Basic tests
     */

    @Test
    public void projectConfiguration() throws Exception {
        ProjectConfiguration c = project_client.getProjectConfiguration();
        assertEquals(c.getName(), "Project 1");
    }

    @Test(expected = RepositoryException.class)
    public void createBranchInvalidProject() throws Exception {
        URI uri = UriBuilder.fromUri("http://localhost/invalid_project").port(port).build();
        project_client = factory.getProjectClient(uri);
        project_client.createBranch(user, "branch1");
    }

    @Test
    public void createBranch() throws Exception {
        project_client.createBranch(user, "branch1");
        BranchStatus branch_status;

        branch_status = project_client.getBranchStatus(user, "branch1");
        assertEquals("branch1", branch_status.getName());
        branch_status = branch_client.getBranchStatus();
        assertEquals("branch1", branch_status.getName());

        BranchList list = project_client.getBranchList(user);
        assertEquals("branch1", list.getBranchesList().get(0));

        project_client.deleteBranch(user, "branch1");
        list = project_client.getBranchList(user);
        assertEquals(0, list.getBranchesCount());
    }

    @Test(expected = RepositoryException.class)
    public void createBranchTwice() throws Exception {
        project_client.createBranch(user, "branch1");
        project_client.createBranch(user, "branch1");
    }

    @Test(expected = RepositoryException.class)
    public void deleteNonExistant() throws Exception {
        project_client.deleteBranch(user, "branch1");
    }

    @Test
    public void makeDirAddCommitRemove() throws Exception {
        project_client.createBranch(user, "branch1");
        branch_client.mkdir("/content/foo");
        branch_client.mkdir("/content/foo");

        BranchStatus branch = branch_client.getBranchStatus();
        assertEquals(Protocol.BranchStatus.State.CLEAN, branch.getBranchState());

        branch_client.putResourceData("/content/foo/bar.txt", "bar data".getBytes());

        branch = branch_client.getBranchStatus();
        assertEquals(Protocol.BranchStatus.State.DIRTY, branch.getBranchState());

        assertEquals("bar data", new String(branch_client.getResourceData("/content/foo/bar.txt")));
        branch_client.commit("message...");

        branch = branch_client.getBranchStatus();
        assertEquals(Protocol.BranchStatus.State.CLEAN, branch.getBranchState());

        branch_client.deleteResource("/content/foo/bar.txt");
        branch = branch_client.getBranchStatus();
        assertEquals(Protocol.BranchStatus.State.DIRTY, branch.getBranchState());

        branch_client.commit("message...");

        branch = branch_client.getBranchStatus();
        assertEquals(Protocol.BranchStatus.State.CLEAN, branch.getBranchState());
    }

    @Test
    public void getResourceNotFound() throws RepositoryException {
        project_client.createBranch(user, "branch1");
        try {
            branch_client.getResourceInfo("/content/does_not_exists");
            assertTrue(false);
        } catch (RepositoryException e) {
            assertEquals(404, e.getStatusCode());
            e.printStackTrace();
        }

        try {
            branch_client.getResourceData("/content/does_not_exists");
            assertTrue(false);
        } catch (RepositoryException e) {
            assertEquals(404, e.getStatusCode());
            e.printStackTrace();
        }

        try {
            branch_client.getResourceData("/content/content");
            assertTrue(false);
        } catch (RepositoryException e) {
            assertEquals(404, e.getStatusCode());
            e.printStackTrace();
        }

}

    @Test
    public void getResource() throws Exception {
        project_client.createBranch(user, "branch1");

        {
            String local_path = "tmp/branch_root/proj1/john/branch1/content/file1.txt";
            long expected_size = new File(local_path).length();
            long expected_last_mod = new File(local_path).lastModified();

            ResourceInfo info = branch_client.getResourceInfo("/content/file1.txt");
            assertEquals(ResourceType.FILE, info.getType());
            assertEquals("file1.txt", info.getName());
            assertEquals("/content/file1.txt", info.getPath());
            assertEquals(expected_size, info.getSize());
            assertEquals(expected_last_mod, info.getLastModified());
        }

        {
            String local_path = "tmp/branch_root/proj1/john/branch1/content/file1.txt";
            long expected_size = new File(local_path).length();

            byte[] data = branch_client.getResourceData("/content/file1.txt");
            assertEquals(expected_size, data.length);
            assertEquals("file1 data\n", new String(data));
        }

        {
            try {
                branch_client.getResourceData("/content");
            } catch (RepositoryException e) {
                assertEquals(400, e.getStatusCode());
                e.printStackTrace();
            }
        }

        {
            String local_path = "tmp/branch_root/proj1/john/branch1/content";
            long expected_last_mod = new File(local_path).lastModified();

            ResourceInfo info = branch_client.getResourceInfo("/content");
            assertEquals(ResourceType.DIRECTORY, info.getType());
            assertEquals("content", info.getName());
            assertEquals("/content", info.getPath());
            assertEquals(0, info.getSize());
            assertEquals(expected_last_mod, info.getLastModified());
            Set<String> expected_set = new HashSet<String>(Arrays.asList(new String[] { "file1.txt", "file2.txt" }));
            Set<String> actual_set = new HashSet<String>(info.getSubResourceNamesList());
            assertEquals(expected_set, actual_set);
        }
    }

    /*
     * Branch tests. Update, commit, etc
     */

    @Test
    public void dirtyBranch() throws RepositoryException {
        project_client.createBranch(user, "branch1");

        // Check that branch is clean
        BranchStatus branch = branch_client.getBranchStatus();
        assertEquals(Protocol.BranchStatus.State.CLEAN, branch.getBranchState());

        byte[] old_data = branch_client.getResourceData("/content/file1.txt");

        // Update resource
        branch_client.putResourceData("/content/file1.txt", "new file1 data".getBytes());

        // Check that branch is dirty
        branch = branch_client.getBranchStatus();
        assertEquals(Protocol.BranchStatus.State.DIRTY, branch.getBranchState());

        // Update resource again with original data
        branch_client.putResourceData("/content/file1.txt", old_data);

        // Assert clean state
        branch = branch_client.getBranchStatus();
        assertEquals(Protocol.BranchStatus.State.CLEAN, branch.getBranchState());

        // Update resource again
        branch_client.putResourceData("/content/file1.txt", "new file1 data".getBytes());
        // Revert changes
        branch_client.revertResource("/content/file1.txt");
        // Assert clean state
        branch = branch_client.getBranchStatus();
        assertEquals(Protocol.BranchStatus.State.CLEAN, branch.getBranchState());

        // Create a new resource, delete later
        branch_client.putResourceData("/content/new_file.txt", "new file data".getBytes());

        // Check that branch is dirty
        branch = branch_client.getBranchStatus();
        assertEquals(Protocol.BranchStatus.State.DIRTY, branch.getBranchState());

        // Delete new resource
        branch_client.deleteResource("/content/new_file.txt");
        // Assert clean state
        branch = branch_client.getBranchStatus();
        assertEquals(Protocol.BranchStatus.State.CLEAN, branch.getBranchState());

        // Create a new resource, revert later
        branch_client.putResourceData("/content/new_file.txt", "new file data".getBytes());

        // Check that branch is dirty
        branch = branch_client.getBranchStatus();
        assertEquals(Protocol.BranchStatus.State.DIRTY, branch.getBranchState());

        // Rename file
        branch_client.renameResource("/content/new_file.txt", "/content/new_file2.txt");
        assertEquals(1, branch_client.getBranchStatus().getFileStatusList().size());

        // Revert new resource
        branch_client.revertResource("/content/new_file2.txt");
        // Assert clean state
        branch = branch_client.getBranchStatus();
        assertEquals(Protocol.BranchStatus.State.CLEAN, branch.getBranchState());

        // Create a new resource, revert later
        branch_client.putResourceData("/content/new_file.txt", "new file data".getBytes());

        // Check that branch is dirty
        branch = branch_client.getBranchStatus();
        assertEquals(Protocol.BranchStatus.State.DIRTY, branch.getBranchState());

        // Rename file
        branch_client.renameResource("/content", "/content2");
        // Check that branch is dirty
        branch = branch_client.getBranchStatus();
        assertEquals(Protocol.BranchStatus.State.DIRTY, branch.getBranchState());
        // 3 files under /content
        assertEquals(3, branch_client.getBranchStatus().getFileStatusList().size());

        // Rename back
        branch_client.renameResource("/content2", "/content");
        // Revert new resource
        branch_client.revertResource("/content/new_file.txt");
        // Assert clean state
        branch = branch_client.getBranchStatus();
        for (Status s : branch_client.getBranchStatus().getFileStatusList()) {
            System.out.println(String.format("%s %s", s.getStatus(), s.getName()));
        }
        assertEquals(Protocol.BranchStatus.State.CLEAN, branch.getBranchState());
    }

    @Test
    public void updateBranch() throws IOException, RepositoryException {
        // Create branch
        project_client.createBranch(user, "branch1");

        byte[] old_data = branch_client.getResourceData("/content/file1.txt");
        assertTrue(new String(old_data).indexOf("testing") == -1);

        // Add commit
        execCommand("scripts/add_testdata_proj1_commit.sh");

        // Update branch
        BranchStatus branch = branch_client.update();
        assertEquals(Protocol.BranchStatus.State.CLEAN, branch.getBranchState());

        // Check clean
        branch = branch_client.getBranchStatus();
        assertEquals(Protocol.BranchStatus.State.CLEAN, branch.getBranchState());

        byte[] new_data = branch_client.getResourceData("/content/file1.txt");
        assertTrue(new String(new_data).indexOf("testing") != -1);
    }

    @Test
    public void updateBranchMergeResolveYours() throws IOException, RepositoryException {
        // Create branch
        project_client.createBranch(user, "branch1");

        // Update resource
        branch_client.putResourceData("/content/file1.txt", "new file1 data".getBytes());

        // Add commit in main branch
        execCommand("scripts/add_testdata_proj1_commit.sh");

        // Commit in this branch
        branch_client.commit("test message");

        // Update branch
        BranchStatus branch = branch_client.update();
        assertEquals(Protocol.BranchStatus.State.MERGE, branch.getBranchState());
        assertEquals("/content/file1.txt", branch.getFileStatus(0).getName());
        assertEquals("U", branch.getFileStatus(0).getStatus());

        // Check merge state
        branch = branch_client.getBranchStatus();
        assertEquals(Protocol.BranchStatus.State.MERGE, branch.getBranchState());

        byte[] new_data = branch_client.getResourceData("/content/file1.txt");
        assertTrue(new String(new_data).indexOf("<<<<<<< HEAD") != -1);

        branch_client.resolve("/content/file1.txt", "yours");

        byte[] data = branch_client.getResourceData("/content/file1.txt");
        assertTrue(new String(data).indexOf("<<<<<<< HEAD") == -1);

        // Commit in this branch
        branch_client.commitMerge("test message");

        // Publish this branch
        branch_client.publish();

        // Make changes visible (in file system)
        execCommand("scripts/reset_testdata_proj1.sh");

        String file1 = FileUtil.readEntireFile(new File("tmp/test_data/proj1/content/file1.txt"));
        assertTrue(file1.indexOf("new file1 data") != -1);
    }

    @Test
    public void updateBranchMergeResolveTheirs() throws IOException, RepositoryException {
        // Create branch
        project_client.createBranch(user, "branch1");

        // Update resource
        branch_client.putResourceData("/content/file1.txt", "new file1 data".getBytes());

        // Add commit in main branch
        execCommand("scripts/add_testdata_proj1_commit.sh");

        // Commit in this branch
        branch_client.commit("test message");

        // Update branch
        BranchStatus branch = branch_client.update();
        assertEquals(Protocol.BranchStatus.State.MERGE, branch.getBranchState());
        assertEquals("/content/file1.txt", branch.getFileStatus(0).getName());
        assertEquals("U", branch.getFileStatus(0).getStatus());

        // Check merge state
        branch = branch_client.getBranchStatus();
        assertEquals(Protocol.BranchStatus.State.MERGE, branch.getBranchState());

        byte[] new_data = branch_client.getResourceData("/content/file1.txt");
        assertTrue(new String(new_data).indexOf("<<<<<<< HEAD") != -1);

        branch_client.resolve("/content/file1.txt", "theirs");

        byte[] data = branch_client.getResourceData("/content/file1.txt");
        assertTrue(new String(data).indexOf("<<<<<<< HEAD") == -1);

        // Commit in this branch
        branch_client.commitMerge("test message");

        // Publish this branch
        branch_client.publish();

        // Make changes visible (in file system)
        execCommand("scripts/reset_testdata_proj1.sh");

        String file1 = FileUtil.readEntireFile(new File("tmp/test_data/proj1/content/file1.txt"));
        assertTrue(file1.indexOf("new file1 data") == -1);
    }

    @Test
    public void publishUnmerged() throws IOException, RepositoryException {
        // Create branch
        project_client.createBranch(user, "branch1");

        // Update resource
        branch_client.putResourceData("/content/file1.txt", "new file1 data".getBytes());

        // Add commit in main branch
        execCommand("scripts/add_testdata_proj1_commit.sh");

        // Commit in this branch
        branch_client.commit("my commit message");

        // Update branch
        BranchStatus branch = branch_client.update();
        assertEquals(Protocol.BranchStatus.State.MERGE, branch.getBranchState());

        // Check merge state
        branch = branch_client.getBranchStatus();
        assertEquals(Protocol.BranchStatus.State.MERGE, branch.getBranchState());

        byte[] new_data = branch_client.getResourceData("/content/file1.txt");
        assertTrue(new String(new_data).indexOf("<<<<<<< HEAD") != -1);

        // Publish this branch
        try {
            branch_client.publish();
        }
        catch (RepositoryException e) {
            assertEquals(400, e.getStatusCode());
        }
    }

}
