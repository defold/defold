package com.dynamo.cr.server.resources;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.URI;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Collection;
import java.util.List;
import java.util.Set;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

import javax.annotation.security.RolesAllowed;
import javax.ws.rs.Consumes;
import javax.ws.rs.DELETE;
import javax.ws.rs.DefaultValue;
import javax.ws.rs.GET;
import javax.ws.rs.POST;
import javax.ws.rs.PUT;
import javax.ws.rs.Path;
import javax.ws.rs.PathParam;
import javax.ws.rs.Produces;
import javax.ws.rs.QueryParam;
import javax.ws.rs.WebApplicationException;
import javax.ws.rs.core.Context;
import javax.ws.rs.core.MediaType;
import javax.ws.rs.core.Response;
import javax.ws.rs.core.Response.Status;
import javax.ws.rs.core.StreamingOutput;
import javax.ws.rs.core.UriInfo;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.IOUtils;
import org.apache.commons.io.filefilter.AbstractFileFilter;
import org.apache.commons.io.filefilter.IOFileFilter;
import org.apache.commons.io.filefilter.TrueFileFilter;
import org.eclipse.jgit.api.CloneCommand;
import org.eclipse.jgit.api.Git;
import org.eclipse.jgit.api.errors.InvalidRefNameException;
import org.eclipse.jgit.api.errors.JGitInternalException;
import org.eclipse.jgit.api.errors.RefAlreadyExistsException;
import org.eclipse.jgit.api.errors.RefNotFoundException;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.branchrepo.BranchRepositoryException;
import com.dynamo.cr.proto.Config.Application;
import com.dynamo.cr.proto.Config.Configuration;
import com.dynamo.cr.protocol.proto.Protocol.ApplicationInfo;
import com.dynamo.cr.protocol.proto.Protocol.BranchList;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus;
import com.dynamo.cr.protocol.proto.Protocol.BuildDesc;
import com.dynamo.cr.protocol.proto.Protocol.BuildLog;
import com.dynamo.cr.protocol.proto.Protocol.CommitDesc;
import com.dynamo.cr.protocol.proto.Protocol.LaunchInfo;
import com.dynamo.cr.protocol.proto.Protocol.Log;
import com.dynamo.cr.protocol.proto.Protocol.ProjectInfo;
import com.dynamo.cr.protocol.proto.Protocol.ResolveStage;
import com.dynamo.cr.protocol.proto.Protocol.ResourceInfo;
import com.dynamo.cr.server.Server;
import com.dynamo.cr.server.ServerException;
import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.Project;
import com.dynamo.cr.server.model.User;
import com.dynamo.inject.persist.Transactional;
import com.dynamo.server.dgit.GitFactory;
import com.dynamo.server.dgit.GitFactory.Type;
import com.dynamo.server.dgit.IGit;
import com.google.common.io.Files;
import com.sun.jersey.api.NotFoundException;

/*
 * use
 * # curl -X METHOD URI
 * to test. More about curl-testing: http://blogs.sun.com/enterprisetechtips/entry/consuming_restful_web_services_with
 *
 * Root URI:
 *
 * http://host/.../project/user/branch
 *
 *
 * (( NOT VALID ANYMORE?!?
 * NOTE: In order avoid ambiguity service names are reserved words, ie
 * no branch names must not contain any of the reserved words (TODO: unit-test reminder for this)))
 *
 * NOTE: Directories are created using resources-put below with directory=true. TODO: Better solution?
 * NOTE: Service name in upper-case below for clarity
 *
 * Branches:
 *   ProjectConfiguration:
 *       GET http://host/project
 *   Get application info:
 *       TODO: application_info should be changed to application/info. Same for application_data below.
 *       Currently we get a clash with branch resources. Prepend /branch to all branch resources?
 *       GET http://host/project/application_info?platform=(win32|linux|...)
 *   Get application data:
 *       GET http://host/project/application_data?platform=(win32|linux|...)
 *   List:
 *       GET http://host/project/user
 *   Info:
 *       GET http://host/project/user/branch
 *   Create:
 *       PUT http://host/project/user/branch
 *   Delete:
 *       DELETE http://host/project/user/branch
 *   Update:
 *       POST http://host/project/user/branch/UPDATE
 *   Commit:
 *       POST http://host/project/user/branch/COMMIT?all=(true|false)
 *   Publish:
 *       POST http://host/project/user/branch/PUBLISH
 *   Resolve merge conflict:
 *       PUT http://host/project/user/branch/RESOLVE?stage=(base|yours|theirs)&path=resource_path
 *
 * Resources:
 *   Info:
 *       GET http://host/project/user/branch/RESOURCES/INFO?path=resource_path
 *   Content:
 *       GET http://host/project/user/branch/RESOURCES/DATA?path=resource_path
 *       PUT http://host/project/user/branch/RESOURCES/DATA?path=resource_path?directory=(true|false)
 *       POST http://host/project/user/branch/RESOURCES/DATA?source=resource_path?destination=resource_path (rename)
 *       DELETE http://host/project/user/branch/RESOURCES/DATA?path=resource_path
 *
 *   NOTE: Resource path above is rooted
 *
 * Builds:
 *   NOTE: Builds-URI:s seems to be ambiguous to branch-info above. Read the spec to ensure that
 *   this is not the case (jax-rs spec).
 *
 *   Build:
 *       POST http://host/project/user/branch/BUILDS?rebuild=(true|false)
 *   Build info:
 *       GET http://host/project/user/branch/BUILDS?id=integer
 *
 *  Extensions:
 *  Commits:
 *    Log:
 *       GET http://host/project/user/branch/commits?max=integer
 *
 */

// NOTE: {member} isn't currently used.
// See README.md in this project for additional information
@Path("/projects/{owner}/{project}")
@RolesAllowed(value = { "member" })
public class ProjectResource extends BaseResource {

    protected static Logger logger = LoggerFactory.getLogger(ProjectResource.class);

    /*
     * Project
     */
    private static void zipFiles(File directory, File zipFile) throws IOException {
        ZipOutputStream zipOut = null;

        try {
            zipOut = new ZipOutputStream(new FileOutputStream(zipFile));
            IOFileFilter dirFilter = new AbstractFileFilter() {
                @Override
                public boolean accept(File file) {
                    return !file.getName().equals(".git");
                }
            };

            Collection<File> files = FileUtils.listFiles(directory, TrueFileFilter.INSTANCE, dirFilter);
            int prefixLength = directory.getAbsolutePath().length();
            for (File file : files) {
                String p = file.getAbsolutePath().substring(prefixLength);
                if (p.startsWith("/")) {
                    p = p.substring(1);
                }
                ZipEntry ze = new ZipEntry(p);
                zipOut.putNextEntry(ze);
                FileUtils.copyFile(file, zipOut);
            }
        } finally {
            IOUtils.closeQuietly(zipOut);
        }
    }

    @GET
    @Path("/archive")
    @RolesAllowed(value = { "member" })
    public Response getArchive(@PathParam("project") String project) throws IOException {
        return getArchive(project, "HEAD");
    }

    @GET
    @Path("/archive/{version}")
    @RolesAllowed(value = { "member" })
    public Response getArchive(@PathParam("project") String project,
                               @PathParam("version") String version) throws IOException {

        // NOTE: Currently neither caching of created zip-files nor appropriate cache-headers (ETag)
        // Appropriate ETag might be SHA1 of zip-file or even SHA1 from git (head commit)

        String repository = String.format("%s/%s", server.getConfiguration().getRepositoryRoot(), project);
        File cloneTo = null;
        final File zipFile = File.createTempFile("archive", ".zip");

        try {
            cloneTo = Files.createTempDir();
            // NOTE: No shallow clone support in current JGit
            CloneCommand clone = Git.cloneRepository()
                    .setURI(repository)
                    .setBare(false)
                    .setDirectory(cloneTo);
            Git git = clone.call();

            try {
                git.checkout().setName(version).call();
            } catch (JGitInternalException e) {
                throw new ServerException("Failed to checkout repo", e, Status.INTERNAL_SERVER_ERROR);
            } catch (RefAlreadyExistsException e) {
                throw new ServerException("Failed to checkout repo", e, Status.INTERNAL_SERVER_ERROR);
            } catch (RefNotFoundException e) {
                throw new ServerException(String.format("Version not found: %s", version), e, Status.INTERNAL_SERVER_ERROR);
            } catch (InvalidRefNameException e) {
                throw new ServerException(String.format("Version not found: %s", version), e, Status.INTERNAL_SERVER_ERROR);
            }

            zipFiles(cloneTo, zipFile);
        } catch (IOException e) {
            // NOTE: Only delete zip-file on error as we need the file
            // when streaming. See .delete() in StreamingOutput below.
            zipFile.delete();
            throw new IOException("Failed to clone and zip project", e);
        } finally {
            // NOTE: We always delete temporary cloneTo directory
            try {
                FileUtils.deleteDirectory(cloneTo);
            } catch (IOException e) {
                logger.error("Failed to remove temporary clone directory", e);
            }
        }

        StreamingOutput output = new StreamingOutput() {
            @Override
            public void write(OutputStream os) throws IOException, WebApplicationException {
                FileUtils.copyFile(zipFile, os);
                os.close();
                zipFile.delete();
            }
        };

        return Response.ok(output, MediaType.APPLICATION_OCTET_STREAM_TYPE)
                .header("content-disposition", String.format("attachment; filename = archive_%s_%s", project, version))
                .build();
    }

    @GET
    @Path("/launch_info")
    public LaunchInfo getLaunchInfo(@PathParam("project") String project) {
        LaunchInfo ret = server.getLaunchInfo(em, project);
        return ret;
    }

    @GET
    @Path("/application_data")
    public byte[] getApplicationData(@PathParam("project") String project,
                                     @QueryParam("platform") String platform) throws IOException {
        Configuration config = server.getConfiguration();

        List<Application> applications = config.getApplicationsList();
        for (Application application : applications) {
            if (application.getPlatform().equals(platform)) {
                File f = new File(application.getPath());
                byte[] buffer = new byte[(int) f.length()];
                FileInputStream is = new FileInputStream(f);
                is.read(buffer);
                is.close();
                return buffer;
            }
        }
        throw new NotFoundException(String.format("Application for platform %s not found", platform));
    }

    @GET
    @Path("/application_info")
    public ApplicationInfo getApplicationInfo(@PathParam("project") String project,
                                              @QueryParam("platform") String platform) throws IOException {
        Configuration config = server.getConfiguration();
        List<Application> applications = config.getApplicationsList();
        for (Application application : applications) {
            if (application.getPlatform().equals(platform)) {
                File f = new File(application.getPath());
                byte[] buffer = new byte[(int) f.length()];

                MessageDigest md = null;
                try {
                    md = MessageDigest.getInstance("SHA-1");
                } catch (NoSuchAlgorithmException e) {
                    throw new RuntimeException(e);
                }

                FileInputStream is = new FileInputStream(f);
                is.read(buffer);
                md.update(buffer);
                byte[] digest = md.digest();
                StringBuffer hexDigest = new StringBuffer();
                for (byte b : digest) {
                    hexDigest.append(Integer.toHexString(0xFF & b));
                }

                is.close();

                ApplicationInfo ret = ApplicationInfo.newBuilder()
                    .setName(application.getName())
                    .setVersion(hexDigest.toString())
                    .setSize((int) f.length())
                    .build();
                return ret;
            }
        }
        throw new NotFoundException(String.format("Application for platform %s not found", platform));
    }

    @POST
    @Path("/members")
    @RolesAllowed(value = { "member" })
    @Transactional
    public void addMember(@PathParam("project") String projectId,
                          String memberEmail) {
        User user = getUser();
        User member = ModelUtil.findUserByEmail(em, memberEmail);
        if (member == null) {
            throw new ServerException("User not found", Status.NOT_FOUND);
        }

        // Connect new member to owner (used in e.g. auto-completion)
        user.getConnections().add(member);

        Project project = server.getProject(em, projectId);
        project.getMembers().add(member);
        member.getProjects().add(project);
        em.persist(project);
        em.persist(user);
        em.persist(member);
    }

    @DELETE
    @Path("/members/{id}")
    @Transactional
    public void removeMember(@PathParam("project") String projectId,
                          @PathParam("id") String memberId) {
        // Ensure user is valid
        User user = getUser();
        User member = server.getUser(em, memberId);

        Project project = server.getProject(em, projectId);

        if (!project.getMembers().contains(member)) {
            throw new NotFoundException(String.format("User %s is not a member of project %s", user.getId(), projectId));
        }
        // Only owners can remove other users and only non-owners can remove themselves
        boolean isOwner = user.getId() == project.getOwner().getId();
        boolean removeSelf = user.getId() == member.getId();
        if ((isOwner && removeSelf) || (!isOwner && !removeSelf)) {
            throw new WebApplicationException(403);
        }

        ModelUtil.removeMember(project, member);
        em.persist(project);
        em.persist(member);
    }

    @GET
    @Path("/project_info")
    public ProjectInfo getProjectInfo(@PathParam("project") String projectId) {
        User user = getUser();
        Project project = server.getProject(em, projectId);
        return ResourceUtil.createProjectInfo(server.getConfiguration(), user, project,
                ModelUtil.isMemberQualified(em, user, project, server.getConfiguration().getProductsList()));
    }

    @PUT
    @RolesAllowed(value = { "owner" })
    @Transactional
    @Path("/project_info")
    public void updateProjectInfo(@PathParam("project") String projectId,
                                  ProjectInfo projectInfo) {
        /*
         * Only name and description is updated
         */

        Project project = server.getProject(em, projectId);
        project.setName(projectInfo.getName());
        project.setDescription(projectInfo.getDescription());
    }

    @GET
    @Path("/log")
    public Log log(@PathParam("project") String project,
                              @QueryParam("max_count") int maxCount) throws IOException {
        Log log = server.log(em, project, maxCount);
        return log;
    }

    @DELETE
    @RolesAllowed(value = { "owner" })
    @Transactional
    public void deleteProject(@PathParam("project") String projectId)  {
        Project project = server.getProject(em, projectId);
        Set<User> members = project.getMembers();
        // Remove branches
        for (User member : members) {
            String memberId = member.getId().toString();
            for (String branch : branchRepository.getBranchNames(projectId, memberId)) {
                try {
                    branchRepository.deleteBranch(projectId, memberId, branch);
                } catch (BranchRepositoryException e) {
                    this.throwWebApplicationException(Status.INTERNAL_SERVER_ERROR, e.getMessage());
                }
            }
        }
        // Delete git repo
        Configuration configuration = server.getConfiguration();
        String repositoryRoot = configuration.getRepositoryRoot();
        File projectPath = new File(String.format("%s/%d", repositoryRoot, project.getId()));
        IGit git = GitFactory.create(Type.CGIT);
        try {
            git.rmRepo(projectPath.getAbsolutePath());
            ModelUtil.removeProject(em, project);
        } catch (IOException e) {
            throw new ServerException(String.format("Could not delete git repo for project %s", project.getName()), Status.INTERNAL_SERVER_ERROR);
        }
    }

    /*
     * Defold Engine
     */
    @POST
    @Consumes(MediaType.APPLICATION_OCTET_STREAM)
    @RolesAllowed(value = { "member" })
    @Path("/engine/{platform}")
    public Response uploadEngine(@PathParam("project") String projectId,
                                 @PathParam("platform") String platform,
                                 InputStream stream) throws IOException {

        if (!platform.equals("ios")) {
            // Only iOS for now
            throwWebApplicationException(Status.NOT_FOUND, "Unsupported platform");
        }
        server.uploadEngine(projectId, platform, stream);
        return Response.ok().build();
    }

    @GET
    @RolesAllowed(value = { "anonymous" })
    @Produces(MediaType.APPLICATION_OCTET_STREAM)
    @Path("/engine/{platform}/{key}")
    public byte[] downloadEngine(@PathParam("project") String projectId,
                                 @PathParam("key") String key,
                                 @PathParam("platform") String platform) throws IOException {

        if (!platform.equals("ios")) {
            // Only iOS for now
            throwWebApplicationException(Status.NOT_FOUND, "Unsupported platform");
        }

        Project project = server.getProject(em, projectId);
        String actualKey = Server.getEngineDownloadKey(project);

        if (!key.equals(actualKey)) {
            throwWebApplicationException(Status.FORBIDDEN, "Forbidden");
        }

        return server.downloadEngine(projectId, platform);
    }

    @GET
    @RolesAllowed(value = { "anonymous" })
    @Produces({"text/xml"})
    @Path("/engine_manifest/{platform}/{key}")
    public String downloadEngineManifest(@PathParam("user") String user,
                                         @PathParam("project") String projectId,
                                         @PathParam("key") String key,
                                         @PathParam("platform") String platform,
                                         @Context UriInfo uriInfo) throws IOException {

        if (!platform.equals("ios")) {
            // Only iOS for now
            throwWebApplicationException(Status.NOT_FOUND, "Unsupported platform");
        }

        Project project = server.getProject(em, projectId);
        String actualKey = Server.getEngineDownloadKey(project);

        if (!key.equals(actualKey)) {
            throwWebApplicationException(Status.FORBIDDEN, "Forbidden");
        }

        InputStream stream = this.getClass().getResourceAsStream("ota_manifest.plist");
        String manifest = IOUtils.toString(stream);
        stream.close();

        URI engineUri = uriInfo.getBaseUriBuilder().path("projects").path(user).path(projectId).path("engine").path(platform).path(key).build();
        String manifestPrim = manifest.replace("${URL}", engineUri.toString());
        return manifestPrim;
    }

    /*
     * Branch
     */

    @GET
    @Path("/branches/")
    public BranchList getBranchList(@PathParam("project") String project) {
        String user = Long.toString(getUser().getId());
        return branchRepository.getBranchList(project, user);
    }

    @GET
    @Path("/branches/{branch}")
    public BranchStatus getBranchStatus(@PathParam("project") String project,
                                        @PathParam("branch") String branch) throws IOException, ServerException, BranchRepositoryException {
        String user = Long.toString(getUser().getId());
        BranchStatus ret = branchRepository.getBranchStatus(project, user, branch, true);
        return ret;
    }

    @PUT
    @Path("/branches/{branch}")
    public void createBranch(@PathParam("project") String project,
                             @PathParam("branch") String branch) throws IOException, BranchRepositoryException {
        String user = Long.toString(getUser().getId());
        branchRepository.createBranch(project, user, branch);
    }

    @DELETE
    @Path("/branches/{branch}")
    public void deleteBranch(@PathParam("project") String project,
                             @PathParam("branch") String branch) throws ServerException, BranchRepositoryException {
        String user = Long.toString(getUser().getId());
        branchRepository.deleteBranch(project, user, branch);
    }

    @POST
    @Path("/branches/{branch}/update")
    public BranchStatus updateBranch(@PathParam("project") String project,
                               @PathParam("branch") String branch) throws IOException, BranchRepositoryException {

        String user = Long.toString(getUser().getId());
        branchRepository.updateBranch(project, user, branch);
        BranchStatus ret = getBranchStatus(project, branch);
        return ret;
    }

    @POST
    @Path("/branches/{branch}/commit")
    public CommitDesc commitBranch(@PathParam("project") String project,
                             @PathParam("branch") String branch,
                             @QueryParam("all") boolean all,
                             String message) throws IOException, BranchRepositoryException {

        String user = Long.toString(getUser().getId());
        CommitDesc commit;
        if (all)
            commit = branchRepository.commitBranch(project, user, branch, message);
        else
            commit = branchRepository.commitMergeBranch(project, user, branch, message);
        return commit;
    }

    @POST
    @Path("/branches/{branch}/publish")
    public void publishBranch(@PathParam("project") String project,
                              @PathParam("branch") String branch) throws IOException, BranchRepositoryException {
        String user = Long.toString(getUser().getId());
        branchRepository.publishBranch(project, user, branch);
    }

    @POST
    @Path("/branches/{branch}/resolve")
    public void resolve(@PathParam("project") String project,
                        @PathParam("branch") String branch,
                        @QueryParam("path") String path,
                        @QueryParam("stage") String stage) throws IOException, BranchRepositoryException {
        String user = Long.toString(getUser().getId());
        ResolveStage s;
        if (stage.equals("base"))
            s = ResolveStage.BASE;
        else if (stage.equals("yours"))
            s = ResolveStage.YOURS;
        else if (stage.equals("theirs"))
            s = ResolveStage.THEIRS;
        else
            throw new ServerException(String.format("Unknown stage: %s", stage));

        branchRepository.resolveResource(project, user, branch, path, s);
    }

    /*
     * Resource
     */

    @GET
    @Path("/branches/{branch}/resources/info")
    public ResourceInfo getResourceInfo(@PathParam("project") String project,
                                          @PathParam("branch") String branch,
                                          @QueryParam("path") String path) throws IOException, BranchRepositoryException {

        String user = Long.toString(getUser().getId());
        ResourceInfo ret = branchRepository.getResourceInfo(project, user, branch, path);
        return ret;
    }

    @DELETE
    @Path("/branches/{branch}/resources/info")
    public void deleteResource(@PathParam("project") String project,
                               @PathParam("branch") String branch,
                               @QueryParam("path") String path) throws IOException, BranchRepositoryException {

        String user = Long.toString(getUser().getId());
        branchRepository.deleteResource(project, user, branch, path);
    }

    @POST
    @Path("/branches/{branch}/resources/rename")
    public void renameResource(@PathParam("project") String project,
                               @PathParam("branch") String branch,
                               @QueryParam("source") String source,
                               @QueryParam("destination") String destination) throws IOException, ServerException, BranchRepositoryException {

        String user = Long.toString(getUser().getId());
        branchRepository.renameResource(project, user, branch, source, destination);
    }

    @PUT
    @Path("/branches/{branch}/resources/revert")
    public void revertResource(@PathParam("project") String project,
                               @PathParam("branch") String branch,
                               @QueryParam("path") String path) throws IOException, BranchRepositoryException {

        String user = Long.toString(getUser().getId());
        branchRepository.revertResource(project, user, branch, path);
    }

    @GET
    @Path("/branches/{branch}/resources/data")
    public byte[] getResourceData(@PathParam("project") String project,
                                  @PathParam("branch") String branch,
                                  @QueryParam("path") String path,
                                  @QueryParam("revision") String revision) throws IOException, BranchRepositoryException {

        String user = Long.toString(getUser().getId());
        byte[] ret = branchRepository.getResourceData(project, user, branch, path, revision);
        return ret;
    }

    @PUT
    @Path("/branches/{branch}/resources/data")
    public void putResourceData(@PathParam("project") String project,
                                @PathParam("branch") String branch,
                                @QueryParam("path") String path,
                                @DefaultValue("false") @QueryParam("directory") boolean directory,
                                byte[] data) throws IOException, BranchRepositoryException  {
        String user = Long.toString(getUser().getId());
        if (directory) {
            branchRepository.mkdir(project, user, branch, path);
        }
        else {
            branchRepository.putResourceData(project, user, branch, path, data);
        }
    }

    @POST
    @Path("/branches/{branch}/reset")
    public void reset(@PathParam("project") String project,
                                @PathParam("branch") String branch,
                                @DefaultValue("mixed") @QueryParam("mode") String mode,
                                @QueryParam("target") String target) throws IOException, BranchRepositoryException  {
        String user = Long.toString(getUser().getId());
        branchRepository.reset(project, user, branch, mode, target);
    }

    @GET
    @Path("/branches/{branch}/log")
    public Log logBranch(@PathParam("project") String project,
                              @PathParam("branch") String branch,
                              @QueryParam("max_count") int maxCount) throws IOException, BranchRepositoryException {
        String user = Long.toString(getUser().getId());
        return branchRepository.logBranch(project, user, branch, maxCount);
    }

    /*
     * Builds
     */

    @POST
    @Path("/branches/{branch}/builds")
    public BuildDesc build(@PathParam("project") String project,
                           @PathParam("branch") String branch,
                           @DefaultValue("false") @QueryParam("rebuild") boolean rebuild) {

        String user = Long.toString(getUser().getId());
        return server.build(project, user, branch, rebuild);
    }

    @GET
    @Path("/branches/{branch}/builds")
    public BuildDesc buildsStatus(@PathParam("project") String project,
                                  @PathParam("branch") String branch,
                                  @QueryParam("id") int id) {
        String user = Long.toString(getUser().getId());
        return server.buildStatus(project, user, branch, id);
    }

    @DELETE
    @Path("/branches/{branch}/builds")
    public void cancelBuild(@PathParam("project") String project,
                                 @PathParam("branch") String branch,
                                 @QueryParam("id") int id) {

        String user = Long.toString(getUser().getId());
        server.cancelBuild(project, user, branch, id);
    }

    @GET
    @Path("/branches/{branch}/builds/log")
    public BuildLog buildLog(@PathParam("project") String project,
                              @PathParam("branch") String branch,
                              @QueryParam("id") int id) {

        String user = Long.toString(getUser().getId());
        return server.buildLog(project, user, branch, id);
    }

}
