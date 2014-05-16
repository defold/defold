package com.dynamo.cr.server.resources.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintWriter;
import java.io.StringReader;
import java.net.URI;
import java.net.URL;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashSet;
import java.util.Random;
import java.util.Set;

import javax.persistence.EntityManager;
import javax.ws.rs.core.Response;
import javax.ws.rs.core.UriBuilder;

import org.apache.commons.configuration.plist.XMLPropertyListConfiguration;
import org.apache.commons.io.FileUtils;
import org.apache.commons.io.IOUtils;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameters;

import com.dynamo.cr.client.ClientFactory;
import com.dynamo.cr.client.ClientUtils;
import com.dynamo.cr.client.IBranchClient;
import com.dynamo.cr.client.IClientFactory;
import com.dynamo.cr.client.IClientFactory.BranchLocation;
import com.dynamo.cr.client.IProjectClient;
import com.dynamo.cr.client.IProjectsClient;
import com.dynamo.cr.client.IUsersClient;
import com.dynamo.cr.client.RepositoryException;
import com.dynamo.cr.common.providers.ProtobufProviders;
import com.dynamo.cr.protocol.proto.Protocol;
import com.dynamo.cr.protocol.proto.Protocol.BranchList;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus.Status;
import com.dynamo.cr.protocol.proto.Protocol.BuildDesc;
import com.dynamo.cr.protocol.proto.Protocol.BuildLog;
import com.dynamo.cr.protocol.proto.Protocol.CommitDesc;
import com.dynamo.cr.protocol.proto.Protocol.Log;
import com.dynamo.cr.protocol.proto.Protocol.ProjectInfo;
import com.dynamo.cr.protocol.proto.Protocol.ProjectStatus;
import com.dynamo.cr.protocol.proto.Protocol.ResourceInfo;
import com.dynamo.cr.protocol.proto.Protocol.ResourceType;
import com.dynamo.cr.protocol.proto.Protocol.UserInfo;
import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.Project;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.util.FileUtil;
import com.dynamo.server.dgit.CommandUtil;
import com.sun.jersey.api.client.Client;
import com.sun.jersey.api.client.ClientResponse;
import com.sun.jersey.api.client.WebResource;
import com.sun.jersey.api.client.config.ClientConfig;
import com.sun.jersey.api.client.config.DefaultClientConfig;
import com.sun.jersey.api.client.filter.HTTPBasicAuthFilter;

@RunWith(value = Parameterized.class)
public class ProjectResourceTest extends AbstractResourceTest {

    private BranchLocation branchLocation;
    int port = 6500;

    String ownerEmail = "owner@foo.com";
    String ownerPassword = "secret";
    private User owner;
    private UserInfo ownerInfo;
    private IBranchClient ownerBranchClient;
    private IProjectsClient ownerProjectsClient;
    private IProjectClient ownerProjectClient;
    private IClientFactory ownerFactory;
    private WebResource ownerProjectsWebResource;

    private Project proj1;

    String memberEmail = "member@foo.com";
    String memberPassword = "secret";
    private User member;
    private UserInfo memberInfo;
    private WebResource memberProjectsWebResource;

    String nonMemberEmail = "nonmember@foo.com";
    String nonMemberPassword = "secret";
    private User nonMember;
    private UserInfo nonMemberInfo;
    private WebResource nonMemberProjectsWebResource;

    public ProjectResourceTest(BranchLocation branchLocation) {
        this.branchLocation = branchLocation;
    }

    @Parameters
    public static Collection<BranchLocation[]> data() {
        BranchLocation[][] data = new BranchLocation[][] { { BranchLocation.LOCAL }, { BranchLocation.REMOTE } };
        return Arrays.asList(data);
    }

    void execCommand(String command) throws IOException {
        CommandUtil.Result r = CommandUtil.execCommand(new String[] {"sh", command});
        if (r.exitValue != 0) {
            System.err.println(r.stdOut);
            System.err.println(r.stdErr);
        }
        assertEquals(0, r.exitValue);
    }

    void execCommand(String command, String arg) throws IOException {
        CommandUtil.Result r = CommandUtil.execCommand(new String[] {"/bin/bash", command, arg});
        if (r.exitValue != 0) {
            System.err.println(r.stdOut);
            System.err.println(r.stdErr);
        }
        assertEquals(0, r.exitValue);
    }

    @Before
    public void setUp() throws Exception {
        setupUpTest();

        EntityManager em = emf.createEntityManager();
        em.getTransaction().begin();
        owner = new User();
        owner.setEmail(ownerEmail);
        owner.setFirstName("undefined");
        owner.setLastName("undefined");
        owner.setPassword(ownerPassword);
        em.persist(owner);

        member = new User();
        member.setEmail(memberEmail);
        member.setFirstName("undefined");
        member.setLastName("undefined");
        member.setPassword(memberPassword);
        em.persist(member);

        nonMember = new User();
        nonMember.setEmail(nonMemberEmail);
        nonMember.setFirstName("undefined");
        nonMember.setLastName("undefined");
        nonMember.setPassword(nonMemberPassword);
        em.persist(nonMember);

        proj1 = ModelUtil.newProject(em, owner, "proj1", "proj1 description");
        em.getTransaction().commit();

        ClientConfig cc = new DefaultClientConfig();
        cc.getClasses().add(ProtobufProviders.ProtobufMessageBodyReader.class);
        cc.getClasses().add(ProtobufProviders.ProtobufMessageBodyWriter.class);

        URI uri;
        Client client;
        IUsersClient usersClient;

        uri = UriBuilder.fromUri(String.format("http://localhost/users")).port(port).build();

        client = Client.create(cc);
        client.addFilter(new HTTPBasicAuthFilter(ownerEmail, ownerPassword));
        ownerFactory = new ClientFactory(client, this.branchLocation, "tmp/branch_root", this.ownerEmail, this.ownerPassword);
        usersClient = ownerFactory.getUsersClient(uri);
        ownerInfo = usersClient.getUserInfo(ownerEmail);

        uri = UriBuilder.fromUri(String.format("http://localhost/projects/%d", ownerInfo.getId())).port(port).build();
        ownerProjectsClient = ownerFactory.getProjectsClient(uri);
        uri = UriBuilder.fromUri(String.format("http://localhost/projects/%d/%d", ownerInfo.getId(), proj1.getId())).port(port).build();
        ownerProjectClient = ownerFactory.getProjectClient(uri);
        ownerBranchClient = ownerFactory.getBranchClient(ClientUtils.getBranchUri(ownerProjectClient, "branch1"));
        ownerProjectsWebResource = client.resource(uri);

        client = Client.create(cc);
        client.addFilter(new HTTPBasicAuthFilter(memberEmail, memberPassword));
        memberInfo = usersClient.getUserInfo(memberEmail);
        uri = UriBuilder.fromUri(String.format("http://localhost/projects/%d/%d", memberInfo.getId(), proj1.getId())).port(port).build();
        memberProjectsWebResource = client.resource(uri);

        // Add member
        ownerProjectsWebResource.path("/members").post(memberEmail);

        client = Client.create(cc);
        client.addFilter(new HTTPBasicAuthFilter(nonMemberEmail, nonMemberPassword));
        nonMemberInfo = usersClient.getUserInfo(nonMemberEmail);
        uri = UriBuilder.fromUri(String.format("http://localhost/projects/%d/%d", nonMemberInfo.getId(), proj1.getId())).port(port).build();
        nonMemberProjectsWebResource = client.resource(uri);

        FileUtil.removeDir(new File(server.getBranchRoot()));

        execCommand("scripts/setup_testdata.sh", Long.toString(proj1.getId()));
    }

    @After
    public void tearDown() throws Exception {
    }

    /*
     * Basic tests
     */

    @Test
    public void launchInfo() throws Exception {
        if (branchLocation == BranchLocation.LOCAL) {
            // Launch info is not supported in local branches
            return;
        }
        ownerProjectClient.getLaunchInfo();

        ClientResponse response = memberProjectsWebResource.path("/launch_info").get(ClientResponse.class);
        assertEquals(200, response.getStatus());

        response = nonMemberProjectsWebResource.path("/launch_info").get(ClientResponse.class);
        assertEquals(403, response.getStatus());
    }

    @Test
    public void applicationData() throws Exception {
        ClientResponse response = ownerProjectsWebResource.path("/application_data").queryParam("platform", "linux").get(ClientResponse.class);
        assertEquals(200, response.getStatus());

        response = memberProjectsWebResource.path("/application_data").queryParam("platform", "linux").get(ClientResponse.class);
        assertEquals(200, response.getStatus());

        response = ownerProjectsWebResource.path("/application_data").queryParam("platform", "wii").get(ClientResponse.class);
        assertEquals(404, response.getStatus());

        response = nonMemberProjectsWebResource.path("/application_data").queryParam("platform", "linux").get(ClientResponse.class);
        assertEquals(403, response.getStatus());
    }

    @Test
    public void applicationInfo() throws Exception {
        ClientResponse response = ownerProjectsWebResource.path("/application_info").queryParam("platform", "linux").get(ClientResponse.class);
        assertEquals(200, response.getStatus());

        response = memberProjectsWebResource.path("/application_info").queryParam("platform", "linux").get(ClientResponse.class);
        assertEquals(200, response.getStatus());

        response = ownerProjectsWebResource.path("/application_info").queryParam("platform", "wii").get(ClientResponse.class);
        assertEquals(404, response.getStatus());

        response = nonMemberProjectsWebResource.path("/application_info").queryParam("platform", "linux").get(ClientResponse.class);
        assertEquals(403, response.getStatus());
    }

    @Test
    public void addMember() throws Exception {
        ClientResponse response = nonMemberProjectsWebResource.path("/members").post(ClientResponse.class, nonMemberEmail);
        assertEquals(403, response.getStatus());

        response = memberProjectsWebResource.path("/members").post(ClientResponse.class, nonMemberEmail);
        assertEquals(204, response.getStatus());

        // Add again, verify the list is not increased
        int membersCount = ownerProjectClient.getProjectInfo().getMembersCount();
        assertEquals(3, membersCount);
        memberProjectsWebResource.path("/members").post(nonMemberEmail);
        assertEquals(membersCount, ownerProjectClient.getProjectInfo().getMembersCount());

        response = memberProjectsWebResource.path("/members").post(ClientResponse.class, "nonexisting@foo.com");
        assertEquals(404, response.getStatus());
    }

    @Test
    public void deleteMember() throws Exception {
        ClientResponse response = nonMemberProjectsWebResource.path(String.format("/members/%d", ownerInfo.getId())).delete(ClientResponse.class);
        assertEquals(403, response.getStatus());

        response = memberProjectsWebResource.path(String.format("/members/%d", ownerInfo.getId())).delete(ClientResponse.class);
        assertEquals(403, response.getStatus());
        int b = response.getEntityInputStream().read();
        assertTrue(b >= 0);

        assertEquals(2, ownerProjectClient.getProjectInfo().getMembersCount());
        response = ownerProjectsWebResource.path(String.format("/members/%d", memberInfo.getId())).delete(ClientResponse.class);
        assertEquals(204, response.getStatus());
        assertEquals(1, ownerProjectClient.getProjectInfo().getMembersCount());

        response = ownerProjectsWebResource.path(String.format("/members/%d", memberInfo.getId())).delete(ClientResponse.class);
        assertEquals(404, response.getStatus());

        response = ownerProjectsWebResource.path("/members/9999").delete(ClientResponse.class);
        assertEquals(404, response.getStatus());
    }

    @Test
    public void projectInfo() throws Exception {
        ProjectInfo projectInfo = ownerProjectClient.getProjectInfo();
        assertEquals("proj1", projectInfo.getName());
        // Owner has default free plan and the project has 2 members
        assertThat(projectInfo.getStatus(), is(ProjectStatus.PROJECT_STATUS_UNQUALIFIED));

        ClientResponse response = memberProjectsWebResource.path("/project_info").get(ClientResponse.class);
        assertEquals(200, response.getStatus());

        response = memberProjectsWebResource.path("/project_info").put(ClientResponse.class, projectInfo);
        assertEquals(403, response.getStatus());

        response = nonMemberProjectsWebResource.path("/project_info").get(ClientResponse.class);
        assertEquals(403, response.getStatus());

        response = nonMemberProjectsWebResource.path("/project_info").put(ClientResponse.class, projectInfo);
        assertEquals(403, response.getStatus());

        ProjectInfo newProjectInfo = ProjectInfo.newBuilder()
            .mergeFrom(projectInfo)
            .setName("new name")
            .setDescription("new desc")
            .build();
        ownerProjectClient.setProjectInfo(newProjectInfo);

        projectInfo = ownerProjectClient.getProjectInfo();
        assertEquals("new name", projectInfo.getName());
        assertEquals("new desc", projectInfo.getDescription());
    }

    @Test
    public void deleteProject() throws Exception {
        ownerProjectClient.createBranch("branch1");

        assertEquals(1, ownerProjectsClient.getProjects().getProjectsCount());
        assertEquals(1, ownerProjectClient.getBranchList().getBranchesCount());

        ClientResponse response = memberProjectsWebResource.path("").delete(ClientResponse.class);
        assertEquals(403, response.getStatus());

        response = nonMemberProjectsWebResource.path("").delete(ClientResponse.class);
        assertEquals(403, response.getStatus());

        response = ownerProjectsWebResource.path("").delete(ClientResponse.class);
        assertEquals(204, response.getStatus());

        assertEquals(0, ownerProjectsClient.getProjects().getProjectsCount());
    }

    @Test
    public void uploadEngine() throws Exception {
        if (branchLocation == BranchLocation.REMOTE) {
            // Upload is not supported in remote branches
            return;
        }

        File f = File.createTempFile("test", ".suff");
        f.deleteOnExit();
        FileOutputStream out = new FileOutputStream(f);

        byte[] buf = new byte[12242];
        new Random().nextBytes(buf);
        out.write(buf);
        out.close();
        ownerProjectClient.uploadEngine("ios", new FileInputStream(f));

        byte[] uploaded = FileUtils.readFileToByteArray(new File(String.format("tmp/engine_root/%d/ios/Defold.ipa", proj1.getId())));
        assertArrayEquals(buf, uploaded);

        ProjectInfo projectInfo = ownerProjectClient.getProjectInfo();
        String iOSUrl = projectInfo.getIOSExecutableUrl();
        String[] tmp = iOSUrl.split("/");
        String key = tmp[tmp.length-1];

        byte[] downloaded = ownerProjectClient.downloadEngine("ios", key);
        assertArrayEquals(buf, downloaded);

        String manifestString = ownerProjectClient.downloadEngineManifest("ios", key);

        XMLPropertyListConfiguration manifest = new XMLPropertyListConfiguration();
        manifest.load(new StringReader(manifestString));

        XMLPropertyListConfiguration item = (XMLPropertyListConfiguration) manifest.getList("items").get(0);
        XMLPropertyListConfiguration asset = (XMLPropertyListConfiguration) item.getList("assets").get(0);
        URL url = new URL(asset.getString("url"));
        byte[] downloadedFromManifest = IOUtils.toByteArray(url.openStream());
        assertArrayEquals(buf, downloadedFromManifest);
    }

    @Test
    public void simpleBenchMark() throws Exception {
        if (branchLocation == BranchLocation.LOCAL) {
            // Launch info is not supported in local branches
            return;
        }

        // Warm up the jit
        for (int i = 0; i < 2000; ++i) {
            ownerProjectClient.getLaunchInfo();
        }
        long start = System.currentTimeMillis();
        final int iterations = 1000;
        for (int i = 0; i < iterations; ++i) {
            ownerProjectClient.getLaunchInfo();
        }
        long end = System.currentTimeMillis();

        double elapsed = (end-start);
        System.out.format("simpleBenchMark: %f ms / request\n", elapsed / (iterations));
    }

    @Test(expected = RepositoryException.class)
    public void createBranchInvalidProject() throws Exception {
        URI uri = UriBuilder.fromUri("http://localhost/99999").port(port).build();
        ownerProjectClient = ownerFactory.getProjectClient(uri);
        ownerProjectClient.createBranch("branch1");
    }

    @Test
    public void createBranch() throws Exception {
        ownerProjectClient.createBranch("branch1");
        BranchStatus branch_status;

        branch_status = ownerProjectClient.getBranchStatus("branch1", true);
        assertEquals("branch1", branch_status.getName());
        branch_status = ownerBranchClient.getBranchStatus(true);
        assertEquals("branch1", branch_status.getName());

        BranchList list = ownerProjectClient.getBranchList();
        assertEquals("branch1", list.getBranchesList().get(0));

        ownerProjectClient.deleteBranch("branch1");
        list = ownerProjectClient.getBranchList();
        assertEquals(0, list.getBranchesCount());
    }

    @Test(expected = RepositoryException.class)
    public void createBranchTwice() throws Exception {
        ownerProjectClient.createBranch("branch1");
        ownerProjectClient.createBranch("branch1");
    }

    @Test(expected = RepositoryException.class)
    public void deleteNonExistantBranch() throws Exception {
        ownerProjectClient.deleteBranch("branch1");
    }

    @Test
    public void makeDirAddCommitUpdateRevertRemove() throws Exception {
        ownerProjectClient.createBranch("branch1");
        ownerBranchClient.mkdir("/content/foo");
        ownerBranchClient.mkdir("/content/foo");

        BranchStatus branch = ownerBranchClient.getBranchStatus(true);
        assertEquals(Protocol.BranchStatus.State.CLEAN, branch.getBranchState());

        ownerBranchClient.putResourceData("/content/foo/bar.txt", "bar data".getBytes());

        branch = ownerBranchClient.getBranchStatus(true);
        assertEquals(Protocol.BranchStatus.State.DIRTY, branch.getBranchState());

        assertEquals("bar data", new String(ownerBranchClient.getResourceData("/content/foo/bar.txt", "")));
        CommitDesc commit = ownerBranchClient.commit("message...");
        Log log = ownerBranchClient.log(1);
        assertEquals(commit.getId(), log.getCommits(0).getId());
        assertEquals(owner.getFirstName() + " " + owner.getLastName(), commit.getName());
        assertEquals(owner.getEmail(), commit.getEmail());

        ownerBranchClient.putResourceData("/content/foo/bar.txt", "bar2 data".getBytes());
        assertEquals(Protocol.BranchStatus.State.DIRTY, branch.getBranchState());

        assertEquals("bar2 data", new String(ownerBranchClient.getResourceData("/content/foo/bar.txt", "")));
        assertEquals("bar data", new String(ownerBranchClient.getResourceData("/content/foo/bar.txt", "master")));

        ownerBranchClient.revertResource("/content/foo/bar.txt");
        assertEquals("bar data", new String(ownerBranchClient.getResourceData("/content/foo/bar.txt", "")));

        branch = ownerBranchClient.getBranchStatus(true);
        assertEquals(Protocol.BranchStatus.State.CLEAN, branch.getBranchState());

        ownerBranchClient.deleteResource("/content/foo/bar.txt");
        branch = ownerBranchClient.getBranchStatus(true);
        assertEquals(Protocol.BranchStatus.State.DIRTY, branch.getBranchState());

        ownerBranchClient.commit("message...");

        branch = ownerBranchClient.getBranchStatus(true);
        assertEquals(Protocol.BranchStatus.State.CLEAN, branch.getBranchState());
    }

    @Test
    public void getResourceNotFound() throws RepositoryException {
        ownerProjectClient.createBranch("branch1");
        try {
            ownerBranchClient.getResourceInfo("/content/does_not_exists");
            assertTrue(false);
        } catch (RepositoryException e) {
            assertEquals(404, e.getStatusCode());
        }

        try {
            ownerBranchClient.getResourceData("/content/does_not_exists", "");
            assertTrue(false);
        } catch (RepositoryException e) {
            assertEquals(404, e.getStatusCode());
        }

        try {
            ownerBranchClient.getResourceData("/content/does_not_exists", "does_not_exist");
            assertTrue(false);
        } catch (RepositoryException e) {
            assertEquals(404, e.getStatusCode());
        }

        ownerBranchClient.putResourceData("/content/foo.txt", "foo".getBytes());
        assertEquals(Protocol.BranchStatus.State.DIRTY, ownerBranchClient.getBranchStatus(true).getBranchState());
        assertEquals("foo", new String(ownerBranchClient.getResourceData("/content/foo.txt", "")));
        try {
            ownerBranchClient.getResourceData("/content/foo.txt", "does_not_exist");
            assertTrue(false);
        } catch (RepositoryException e) {
            assertEquals(404, e.getStatusCode());
        } finally {
            ownerBranchClient.deleteResource("/content/foo.txt");
            assertEquals(Protocol.BranchStatus.State.CLEAN, ownerBranchClient.getBranchStatus(true).getBranchState());
        }

        try {
            ownerBranchClient.getResourceData("/content/content", "");
            assertTrue(false);
        } catch (RepositoryException e) {
            assertEquals(404, e.getStatusCode());
        }

        try {
            ownerBranchClient.getResourceData("/content/content", "does_not_exist");
            assertTrue(false);
        } catch (RepositoryException e) {
            assertEquals(404, e.getStatusCode());
        }
    }

    @Test
    public void getResource() throws Exception {
        ownerProjectClient.createBranch("branch1");

        {
            String local_path = String.format("tmp/branch_root/%d/%d/branch1/content/file1.txt", proj1.getId(), ownerInfo.getId());
            long expected_size = new File(local_path).length();
            long expected_last_mod = new File(local_path).lastModified();

            ResourceInfo info = ownerBranchClient.getResourceInfo("/content/file1.txt");
            assertEquals(ResourceType.FILE, info.getType());
            assertEquals("file1.txt", info.getName());
            assertEquals("/content/file1.txt", info.getPath());
            assertEquals(expected_size, info.getSize());
            assertEquals(expected_last_mod, info.getLastModified());
        }

        {
            String local_path = String.format("tmp/branch_root/%d/%d/branch1/content/file1.txt", proj1.getId(), ownerInfo.getId());
            long expected_size = new File(local_path).length();

            byte[] data = ownerBranchClient.getResourceData("/content/file1.txt", "");
            String content = new String(data);
            assertEquals(expected_size, data.length);
            assertEquals("file1 data\n", content);
        }

        {
            try {
                ownerBranchClient.getResourceData("/content", "");
            } catch (RepositoryException e) {
                assertEquals(400, e.getStatusCode());
            }
        }

        {
            String local_path = String.format("tmp/branch_root/%d/%d/branch1/content", proj1.getId(), ownerInfo.getId());
            long expected_last_mod = new File(local_path).lastModified();

            ResourceInfo info = ownerBranchClient.getResourceInfo("/content");
            assertEquals(ResourceType.DIRECTORY, info.getType());
            assertEquals("content", info.getName());
            assertEquals("/content", info.getPath());
            assertEquals(0, info.getSize());
            assertEquals(expected_last_mod, info.getLastModified());
            Set<String> expected_set = new HashSet<String>(Arrays.asList(new String[] { "file1.txt", "file2.txt", "test space.txt" }));
            Set<String> actual_set = new HashSet<String>(info.getSubResourceNamesList());
            assertEquals(expected_set, actual_set);
        }
    }

    @Test
    public void getResourceWithSpace() throws Exception {
        ownerProjectClient.createBranch("branch1");

        {
            ownerBranchClient.getResourceInfo("/content/test space.txt");
        }
    }

    @Test(expected=RepositoryException.class)
    public void getResourceInfoWithRelativePath() throws Exception {
        ownerProjectClient.createBranch("branch1");
        ownerBranchClient.getResourceInfo("/content/../content/file1.txt");
    }

    @Test(expected=RepositoryException.class)
    public void getResourceDataWithRelativePath() throws Exception {
        ownerProjectClient.createBranch("branch1");
        ownerBranchClient.getResourceData("/content/../content/file1.txt", "");
    }

    /*
     * Branch tests. Update, commit, etc
     */

    @Test
    public void dirtyBranch() throws RepositoryException {
        ownerProjectClient.createBranch("branch1");

        // Check that branch is clean
        BranchStatus branch = ownerBranchClient.getBranchStatus(true);
        assertEquals(Protocol.BranchStatus.State.CLEAN, branch.getBranchState());

        byte[] old_data = ownerBranchClient.getResourceData("/content/file1.txt", "");

        // Update resource
        ownerBranchClient.putResourceData("/content/file1.txt", "new file1 data".getBytes());

        // Check that branch is dirty
        branch = ownerBranchClient.getBranchStatus(true);
        assertEquals(Protocol.BranchStatus.State.DIRTY, branch.getBranchState());

        // Update resource again with original data
        ownerBranchClient.putResourceData("/content/file1.txt", old_data);

        // Assert clean state
        branch = ownerBranchClient.getBranchStatus(true);
        assertEquals(Protocol.BranchStatus.State.CLEAN, branch.getBranchState());

        // Update resource again
        ownerBranchClient.putResourceData("/content/file1.txt", "new file1 data".getBytes());
        // Revert changes
        ownerBranchClient.revertResource("/content/file1.txt");
        // Assert clean state
        branch = ownerBranchClient.getBranchStatus(true);
        assertEquals(Protocol.BranchStatus.State.CLEAN, branch.getBranchState());

        // Create a new resource, delete later
        ownerBranchClient.putResourceData("/content/new_file.txt", "new file data".getBytes());

        // Check that branch is dirty
        branch = ownerBranchClient.getBranchStatus(true);
        assertEquals(Protocol.BranchStatus.State.DIRTY, branch.getBranchState());

        // Delete new resource
        ownerBranchClient.deleteResource("/content/new_file.txt");
        // Assert clean state
        branch = ownerBranchClient.getBranchStatus(true);
        assertEquals(Protocol.BranchStatus.State.CLEAN, branch.getBranchState());

        // Create a new resource, revert later
        ownerBranchClient.putResourceData("/content/new_file.txt", "new file data".getBytes());

        // Check that branch is dirty
        branch = ownerBranchClient.getBranchStatus(true);
        assertEquals(Protocol.BranchStatus.State.DIRTY, branch.getBranchState());

        // Rename file
        ownerBranchClient.renameResource("/content/new_file.txt", "/content/new_file2.txt");
        assertEquals(1, ownerBranchClient.getBranchStatus(true).getFileStatusList().size());

        // Revert new resource
        ownerBranchClient.revertResource("/content/new_file2.txt");
        // Assert clean state
        branch = ownerBranchClient.getBranchStatus(true);
        assertEquals(Protocol.BranchStatus.State.CLEAN, branch.getBranchState());

        // Create a new resource, revert later
        ownerBranchClient.putResourceData("/content/new_file.txt", "new file data".getBytes());

        // Check that branch is dirty
        branch = ownerBranchClient.getBranchStatus(true);
        assertEquals(Protocol.BranchStatus.State.DIRTY, branch.getBranchState());

        // Rename file
        ownerBranchClient.renameResource("/content", "/content2");
        // Check that branch is dirty
        branch = ownerBranchClient.getBranchStatus(true);
        assertEquals(Protocol.BranchStatus.State.DIRTY, branch.getBranchState());
        // 4 files under /content
        assertEquals(4, ownerBranchClient.getBranchStatus(true).getFileStatusList().size());

        // Rename back
        ownerBranchClient.renameResource("/content2", "/content");
        // Revert new resource
        ownerBranchClient.revertResource("/content/new_file.txt");
        // Assert clean state
        branch = ownerBranchClient.getBranchStatus(true);
        for (Status s : ownerBranchClient.getBranchStatus(true).getFileStatusList()) {
            System.out.println(String.format("%s %s", s.getIndexStatus(), s.getName()));
        }
        assertEquals(Protocol.BranchStatus.State.CLEAN, branch.getBranchState());
    }

    @Test
    public void updateBranch() throws IOException, RepositoryException {
        // Create branch
        ownerProjectClient.createBranch("branch1");

        byte[] old_data = ownerBranchClient.getResourceData("/content/file1.txt", "");
        assertTrue(new String(old_data).indexOf("testing") == -1);

        // Add commit
        execCommand("scripts/add_testdata_proj1_commit.sh", Long.toString(proj1.getId()));

        // Update branch
        BranchStatus branch = ownerBranchClient.update();
        assertEquals(Protocol.BranchStatus.State.CLEAN, branch.getBranchState());

        // Check clean
        branch = ownerBranchClient.getBranchStatus(true);
        assertEquals(Protocol.BranchStatus.State.CLEAN, branch.getBranchState());

        byte[] new_data = ownerBranchClient.getResourceData("/content/file1.txt", "");
        assertTrue(new String(new_data).indexOf("testing") != -1);
    }

    @Test
    public void updateBranchMergeResolveYours() throws IOException, RepositoryException {
        // Create branch
        ownerProjectClient.createBranch("branch1");

        // Update resource
        ownerBranchClient.putResourceData("/content/file1.txt", "new file1 data".getBytes());

        // Add commit in main branch
        execCommand("scripts/add_testdata_proj1_commit.sh", Long.toString(proj1.getId()));

        // Commit in this branch
        ownerBranchClient.commit("test message");

        // Update branch
        BranchStatus branch = ownerBranchClient.update();
        assertEquals(Protocol.BranchStatus.State.MERGE, branch.getBranchState());
        assertEquals("/content/file1.txt", branch.getFileStatus(0).getName());
        assertEquals("U", branch.getFileStatus(0).getIndexStatus());

        // Check merge state
        branch = ownerBranchClient.getBranchStatus(true);
        assertEquals(Protocol.BranchStatus.State.MERGE, branch.getBranchState());

        byte[] new_data = ownerBranchClient.getResourceData("/content/file1.txt", "");
        assertTrue(new String(new_data).indexOf("<<<<<<< HEAD") != -1);

        ownerBranchClient.resolve("/content/file1.txt", "yours");

        byte[] data = ownerBranchClient.getResourceData("/content/file1.txt", "");
        assertTrue(new String(data).indexOf("<<<<<<< HEAD") == -1);

        // Commit in this branch
        ownerBranchClient.commitMerge("test message");

        // Publish this branch
        ownerBranchClient.publish();

        // Make changes visible (in file system)
        execCommand("scripts/reset_testdata_proj1.sh", Long.toString(proj1.getId()));

        String file1 = FileUtil.readEntireFile(new File(String.format("tmp/test_data/%d/content/file1.txt", proj1.getId())));
        assertTrue(file1.indexOf("new file1 data") != -1);
    }

    @Test
    public void updateBranchMergeResolveTheirs() throws IOException, RepositoryException, InterruptedException {
        // Create branch
        ownerProjectClient.createBranch("branch1");
        // TODO: Sleep here to avoid random failures on server
        // The problem could be related to mtime
        // See bug 825
        Thread.sleep(1000);
        // Update resource
        ownerBranchClient.putResourceData("/content/file1.txt", "new file1 data".getBytes());

        // Add commit in main branch
        execCommand("scripts/add_testdata_proj1_commit.sh", Long.toString(proj1.getId()));

        // Commit in this branch
        ownerBranchClient.commit("test message");

        // Update branch
        BranchStatus branch = ownerBranchClient.update();
        assertEquals(Protocol.BranchStatus.State.MERGE, branch.getBranchState());
        assertEquals("/content/file1.txt", branch.getFileStatus(0).getName());
        assertEquals("U", branch.getFileStatus(0).getIndexStatus());

        // Check merge state
        branch = ownerBranchClient.getBranchStatus(true);
        assertEquals(Protocol.BranchStatus.State.MERGE, branch.getBranchState());

        byte[] new_data = ownerBranchClient.getResourceData("/content/file1.txt", "");
        assertTrue(new String(new_data).indexOf("<<<<<<< HEAD") != -1);

        ownerBranchClient.resolve("/content/file1.txt", "theirs");

        byte[] data = ownerBranchClient.getResourceData("/content/file1.txt", "");
        assertTrue(new String(data).indexOf("<<<<<<< HEAD") == -1);

        // Commit in this branch
        ownerBranchClient.commitMerge("test message");

        // Publish this branch
        ownerBranchClient.publish();

        // Make changes visible (in file system)
        execCommand("scripts/reset_testdata_proj1.sh", Long.toString(proj1.getId()));

        String file1 = FileUtil.readEntireFile(new File(String.format("tmp/test_data/%d/content/file1.txt", proj1.getId())));
        assertTrue(file1.indexOf("new file1 data") == -1);
    }

    @Test
    public void publishUnmerged() throws IOException, RepositoryException {
        // Create branch
        ownerProjectClient.createBranch("branch1");

        // Update resource
        ownerBranchClient.putResourceData("/content/file1.txt", "new file1 data".getBytes());

        // Add commit in main branch
        execCommand("scripts/add_testdata_proj1_commit.sh", Long.toString(proj1.getId()));

        // Commit in this branch
        ownerBranchClient.commit("my commit message");

        // Update branch
        BranchStatus branch = ownerBranchClient.update();
        assertEquals(Protocol.BranchStatus.State.MERGE, branch.getBranchState());

        // Check merge state
        branch = ownerBranchClient.getBranchStatus(true);
        assertEquals(Protocol.BranchStatus.State.MERGE, branch.getBranchState());

        byte[] new_data = ownerBranchClient.getResourceData("/content/file1.txt", "");
        assertTrue(new String(new_data).indexOf("<<<<<<< HEAD") != -1);

        // Publish this branch
        try {
            ownerBranchClient.publish();
        }
        catch (RepositoryException e) {
            assertEquals(400, e.getStatusCode());
        }
    }

    @Test
    public void reset() throws IOException, RepositoryException {
        // Create branch
        ownerProjectClient.createBranch("branch1");

        // Check log
        Log log = ownerBranchClient.log(5);
        assertEquals(1, log.getCommitsCount());
        String target = log.getCommits(0).getId();

        // Update resource
        ownerBranchClient.putResourceData("/content/file1.txt", "new file1 data".getBytes());

        // Commit in this branch
        ownerBranchClient.commit("my commit message");

        // Check log
        log = ownerBranchClient.log(5);
        assertEquals(2, log.getCommitsCount());

        ownerBranchClient.reset("hard", target);

        // Check log
        log = ownerBranchClient.log(5);
        assertEquals(1, log.getCommitsCount());
    }

    @Test
    public void build() throws Exception {
        if (branchLocation == BranchLocation.LOCAL) {
            // Building is not supported in local branches
            return;
        }

        PrintWriter writer = new PrintWriter(new File("tmp/build.sh"));
        writer.println("sleep 0.3");
        writer.println("echo HELLO");
        writer.close();
        Runtime.getRuntime().exec("chmod +x tmp/build.sh");

        ownerProjectClient.createBranch("branch1");
        BuildDesc buildDesc = ownerBranchClient.build(true);
        BuildDesc status = ownerBranchClient.getBuildStatus(buildDesc.getId());
        assertEquals(BuildDesc.Activity.BUILDING, status.getBuildActivity());
        Thread.sleep(1000);
        status = ownerBranchClient.getBuildStatus(buildDesc.getId());
        assertEquals(BuildDesc.Activity.IDLE, status.getBuildActivity());

        assertEquals(BuildDesc.Result.OK, status.getBuildResult());
        BuildLog logs = ownerBranchClient.getBuildLogs(buildDesc.getId());
        System.out.println(logs.getStdOut());
        assertTrue("HELLO not found", logs.getStdOut().indexOf("HELLO") != -1);
    }

    @Test
    public void failingBuild() throws Exception {
        if (branchLocation == BranchLocation.LOCAL) {
            // Building is not supported in local branches
            return;
        }

        PrintWriter writer = new PrintWriter(new File("tmp/build.sh"));
        writer.println("exit 5");
        writer.close();
        Runtime.getRuntime().exec("chmod +x tmp/build.sh");

        ownerProjectClient.createBranch("branch1");
        BuildDesc buildDesc = ownerBranchClient.build(true);
        BuildDesc status = ownerBranchClient.getBuildStatus(buildDesc.getId());
        assertEquals(BuildDesc.Activity.BUILDING, status.getBuildActivity());
        Thread.sleep(1000);
        status = ownerBranchClient.getBuildStatus(buildDesc.getId());
        assertEquals(BuildDesc.Activity.IDLE, status.getBuildActivity());
        assertEquals(BuildDesc.Result.ERROR, status.getBuildResult());
    }

    @Test
    public void cleanupBuilds() throws Exception {
        if (branchLocation == BranchLocation.LOCAL) {
            // Building is not supported in local branches
            return;
        }

        server.setCleanupBuildsInterval(50);
        server.setKeepBuildDescFor(700);

        PrintWriter writer = new PrintWriter(new File("tmp/build.sh"));
        writer.close();
        Runtime.getRuntime().exec("chmod +x tmp/build.sh");

        ownerProjectClient.createBranch("branch1");
        BuildDesc buildDesc = ownerBranchClient.build(true);
        BuildDesc status = ownerBranchClient.getBuildStatus(buildDesc.getId());
        Thread.sleep(500);
        status = ownerBranchClient.getBuildStatus(buildDesc.getId());
        assertEquals(BuildDesc.Activity.IDLE, status.getBuildActivity());
        assertEquals(BuildDesc.Result.OK, status.getBuildResult());

        Thread.sleep(700);
        try {
            status = ownerBranchClient.getBuildStatus(buildDesc.getId());
            assertTrue("Expected exception", false);
        }
        catch (RepositoryException e) {
            assertEquals(404, e.getStatusCode());
            return;
        }
    }

    @Test
    public void cancelBuild() throws Exception {
        if (branchLocation == BranchLocation.LOCAL) {
            // Building is not supported in local branches
            return;
        }

        PrintWriter writer = new PrintWriter(new File("tmp/build.sh"));
        writer.println("echo HELLO");
        writer.println("sleep 1000");
        writer.println("echo DONE");
        writer.close();
        Runtime.getRuntime().exec("chmod +x tmp/build.sh");

        ownerProjectClient.createBranch("branch1");
        BuildDesc buildDesc = ownerBranchClient.build(true);
        BuildDesc status = ownerBranchClient.getBuildStatus(buildDesc.getId());
        Thread.sleep(1200);
        status = ownerBranchClient.getBuildStatus(buildDesc.getId());
        assertEquals(BuildDesc.Activity.BUILDING, status.getBuildActivity());

        ownerBranchClient.cancelBuild(buildDesc.getId());

        status = ownerBranchClient.getBuildStatus(buildDesc.getId());
        assertEquals(BuildDesc.Activity.IDLE, status.getBuildActivity());

        BuildLog logs = ownerBranchClient.getBuildLogs(buildDesc.getId());
        assertTrue("HELLO not found", logs.getStdOut().indexOf("HELLO") != -1);
        assertTrue("DONE *found*", logs.getStdOut().indexOf("DONE") == -1);
    }

    @Test
    public void concurrentBuilds() throws Exception {
        if (branchLocation == BranchLocation.LOCAL) {
            // Building is not supported in local branches
            return;
        }

        PrintWriter writer = new PrintWriter(new File("tmp/build.sh"));
        writer.println("echo HELLO");
        writer.println("sleep 1000");
        writer.println("echo DONE");
        writer.close();
        Runtime.getRuntime().exec("chmod +x tmp/build.sh");

        ownerProjectClient.createBranch("branch1");
        @SuppressWarnings("unused")
        BuildDesc buildDesc1 = ownerBranchClient.build(true);

        try {
            @SuppressWarnings("unused")
            BuildDesc buildDesc2 = ownerBranchClient.build(true);
            assertTrue("Expected exception", false);
        }
        catch (RepositoryException e) {
            assertEquals(Response.Status.CONFLICT.getStatusCode(), e.getStatusCode());
            return;
        }
   }

}
