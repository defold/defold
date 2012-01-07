package com.dynamo.cr.server.resources;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.List;
import java.util.Set;

import javax.annotation.security.RolesAllowed;
import javax.persistence.EntityManager;
import javax.ws.rs.DELETE;
import javax.ws.rs.DefaultValue;
import javax.ws.rs.GET;
import javax.ws.rs.POST;
import javax.ws.rs.PUT;
import javax.ws.rs.Path;
import javax.ws.rs.PathParam;
import javax.ws.rs.QueryParam;
import javax.ws.rs.WebApplicationException;
import javax.ws.rs.core.Response.Status;

import com.dynamo.cr.proto.Config.Application;
import com.dynamo.cr.proto.Config.Configuration;
import com.dynamo.cr.protocol.proto.Protocol;
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
import com.dynamo.cr.server.ServerException;
import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.Project;
import com.dynamo.cr.server.model.User;
import com.dynamo.server.git.Git;
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

@Path("/projects/{user}/{project}")
@RolesAllowed(value = { "member" })
public class ProjectResource extends BaseResource {

    /*
     * Project
     */

    @GET
    @Path("/launch_info")
    public LaunchInfo getLaunchInfo(@PathParam("project") String project) throws ServerException {
        EntityManager em = server.getEntityManagerFactory().createEntityManager();
        LaunchInfo ret = server.getLaunchInfo(em, project);
        em.close();
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
    @RolesAllowed(value = { "owner" })
    public void addMember(@PathParam("project") String projectId,
                          @PathParam("user") String userId,
                          String memberEmail) throws ServerException {
        EntityManager em = server.getEntityManagerFactory().createEntityManager();

        User user = server.getUser(em, userId);
        User member = ModelUtil.findUserByEmail(em, memberEmail);
        if (member == null) {
            throw new ServerException("User not found", Status.NOT_FOUND);
        }

        // Connect new member to owner (used in e.g. auto-completion)
        user.getConnections().add(member);

        Project project = server.getProject(em, projectId);
        em.getTransaction().begin();
        project.getMembers().add(member);
        member.getProjects().add(project);
        em.persist(project);
        em.persist(user);
        em.persist(member);
        em.getTransaction().commit();
    }

    @DELETE
    @Path("/members/{id}")
    public void removeMember(@PathParam("project") String projectId,
                          @PathParam("user") String userId,
                          @PathParam("id") String memberId) throws ServerException {
        EntityManager em = server.getEntityManagerFactory().createEntityManager();

        // Ensure user is valid
        User user = server.getUser(em, userId);
        User member = server.getUser(em, memberId);

        Project project = server.getProject(em, projectId);

        if (!project.getMembers().contains(member)) {
            throw new NotFoundException(String.format("User %s is not a member of project %s", userId, projectId));
        }
        // Only owners can remove other users and only non-owners can remove themselves
        boolean isOwner = user.getId() == project.getOwner().getId();
        boolean removeSelf = user.getId() == member.getId();
        if ((isOwner && removeSelf) || (!isOwner && !removeSelf)) {
            throw new WebApplicationException(403);
        }

        em.getTransaction().begin();
        ModelUtil.removeMember(project, member);
        em.persist(project);
        em.persist(member);
        em.getTransaction().commit();
    }

    @GET
    @Path("/project_info")
    public ProjectInfo getProjectInfo(@PathParam("user") String user,
                              @PathParam("project") String projectId) throws ServerException {
        EntityManager em = server.getEntityManagerFactory().createEntityManager();

        // Ensure user is valid
        server.getUser(em, user);
        Project project = server.getProject(em, projectId);
        return ResourceUtil.createProjectInfo(project);
    }

    @GET
    @Path("/log")
    public Log log(@PathParam("project") String project,
                              @PathParam("user") String user,
                              @QueryParam("max_count") int maxCount) throws ServerException, IOException {
        EntityManager em = server.getEntityManagerFactory().createEntityManager();
        // Ensure user is valid
        server.getUser(em, user);
        return server.log(em, project, maxCount);
    }

    @DELETE
    @RolesAllowed(value = { "owner" })
    public void deleteProject(@PathParam("user") String user,
            @PathParam("project") String projectId) throws ServerException {
        EntityManager em = server.getEntityManagerFactory().createEntityManager();

        // Ensure user is valid
        server.getUser(em, user);
        Project project = server.getProject(em, projectId);
        Set<User> members = project.getMembers();
        // Remove branches
        for (User member : members) {
            String memberId = member.getId().toString();
            for (String branch : server.getBranchNames(em, projectId, memberId)) {
                server.deleteBranch(em, projectId, memberId, branch);
            }
        }
        // Delete git repo
        Configuration configuration = server.getConfiguration();
        String repositoryRoot = configuration.getRepositoryRoot();
        File projectPath = new File(String.format("%s/%d", repositoryRoot, project.getId()));
        Git git = new Git();
        try {
            git.rmRepo(projectPath.getAbsolutePath());
            em.getTransaction().begin();
            ModelUtil.removeProject(em, project);
            em.getTransaction().commit();
        } catch (IOException e) {
            throw new ServerException(String.format("Could not delete git repo for project %s", project.getName()), Status.INTERNAL_SERVER_ERROR);
        }
    }

    /*
     * Branch
     */

    @GET
    @Path("/branches/")
    public BranchList getBranchList(@PathParam("project") String project,
                                    @PathParam("user") String user) {
        EntityManager em = server.getEntityManagerFactory().createEntityManager();
        String[] branch_names = server.getBranchNames(em, project, user);
        Protocol.BranchList.Builder b = Protocol.BranchList.newBuilder();
        for (String branch : branch_names) {
            b.addBranches(branch);
        }
        em.close();
        return b.build();
    }

    @GET
    @Path("/branches/{branch}")
    public BranchStatus getBranchStatus(@PathParam("project") String project,
                                        @PathParam("user") String user,
                                        @PathParam("branch") String branch) throws IOException, ServerException {
        EntityManager em = server.getEntityManagerFactory().createEntityManager();
        BranchStatus ret = server.getBranchStatus(em, project, user, branch);
        em.close();
        return ret;
    }

    @PUT
    @Path("/branches/{branch}")
    public void createBranch(@PathParam("project") String project,
                             @PathParam("user") String user,
                             @PathParam("branch") String branch) throws IOException, ServerException {
        EntityManager em = server.getEntityManagerFactory().createEntityManager();
        server.createBranch(em, project, user, branch);
        em.close();
    }

    @DELETE
    @Path("/branches/{branch}")
    public void deleteBranch(@PathParam("project") String project,
                             @PathParam("user") String user,
                             @PathParam("branch") String branch) throws ServerException {
        EntityManager em = server.getEntityManagerFactory().createEntityManager();
        server.deleteBranch(em, project, user, branch);
        em.close();
    }

    @POST
    @Path("/branches/{branch}/update")
    public BranchStatus updateBranch(@PathParam("project") String project,
                               @PathParam("user") String user,
                               @PathParam("branch") String branch) throws IOException, ServerException {

        EntityManager em = server.getEntityManagerFactory().createEntityManager();
        server.updateBranch(em, project, user, branch);
        BranchStatus ret = server.getBranchStatus(em, project, user, branch);
        em.close();
        return ret;
    }

    @POST
    @Path("/branches/{branch}/commit")
    public CommitDesc commitBranch(@PathParam("project") String project,
                             @PathParam("user") String user,
                             @PathParam("branch") String branch,
                             @QueryParam("all") boolean all,
                             String message) throws IOException, ServerException {

        EntityManager em = server.getEntityManagerFactory().createEntityManager();
        CommitDesc commit;
        if (all)
            commit = server.commitBranch(em, project, user, branch, message);
        else
            commit = server.commitMergeBranch(em, project, user, branch, message);
        em.close();
        return commit;
    }

    @POST
    @Path("/branches/{branch}/publish")
    public void publishBranch(@PathParam("project") String project,
                              @PathParam("user") String user,
                              @PathParam("branch") String branch) throws IOException, ServerException {

        EntityManager em = server.getEntityManagerFactory().createEntityManager();
        server.publishBranch(em, project, user, branch);
        em.close();
    }

    @POST
    @Path("/branches/{branch}/resolve")
    public void resolve(@PathParam("project") String project,
                        @PathParam("user") String user,
                        @PathParam("branch") String branch,
                        @QueryParam("path") String path,
                        @QueryParam("stage") String stage) throws IOException, ServerException {
        EntityManager em = server.getEntityManagerFactory().createEntityManager();
        ResolveStage s;
        if (stage.equals("base"))
            s = ResolveStage.BASE;
        else if (stage.equals("yours"))
            s = ResolveStage.YOURS;
        else if (stage.equals("theirs"))
            s = ResolveStage.THEIRS;
        else
            throw new ServerException(String.format("Unknown stage: %s", stage));

        server.resolveResource(em, project, user, branch, path, s);
        em.close();
    }

    /*
     * Resource
     */

    @GET
    @Path("/branches/{branch}/resources/info")
    public ResourceInfo getResourceInfo(@PathParam("project") String project,
                                          @PathParam("user") String user,
                                          @PathParam("branch") String branch,
                                          @QueryParam("path") String path) throws IOException, ServerException {

        EntityManager em = server.getEntityManagerFactory().createEntityManager();
        ResourceInfo ret = server.getResourceInfo(em, project, user, branch, path);
        em.close();
        return ret;
    }

    @DELETE
    @Path("/branches/{branch}/resources/info")
    public void deleteResource(@PathParam("project") String project,
                               @PathParam("user") String user,
                               @PathParam("branch") String branch,
                               @QueryParam("path") String path) throws IOException, ServerException {

        EntityManager em = server.getEntityManagerFactory().createEntityManager();
        server.deleteResource(em, project, user, branch, path);
        em.close();
    }

    @POST
    @Path("/branches/{branch}/resources/rename")
    public void renameResource(@PathParam("project") String project,
                               @PathParam("user") String user,
                               @PathParam("branch") String branch,
                               @QueryParam("source") String source,
                               @QueryParam("destination") String destination) throws IOException, ServerException {

        EntityManager em = server.getEntityManagerFactory().createEntityManager();
        server.renameResource(em, project, user, branch, source, destination);
        em.close();
    }

    @PUT
    @Path("/branches/{branch}/resources/revert")
    public void revertResource(@PathParam("project") String project,
                               @PathParam("user") String user,
                               @PathParam("branch") String branch,
                               @QueryParam("path") String path) throws IOException, ServerException {

        EntityManager em = server.getEntityManagerFactory().createEntityManager();
        server.revertResource(em, project, user, branch, path);
        em.close();
    }

    @GET
    @Path("/branches/{branch}/resources/data")
    public byte[] getResourceData(@PathParam("project") String project,
                                  @PathParam("user") String user,
                                  @PathParam("branch") String branch,
                                  @QueryParam("path") String path,
                                  @QueryParam("revision") String revision) throws IOException, ServerException {

        EntityManager em = server.getEntityManagerFactory().createEntityManager();
        byte[] ret = server.getResourceData(em, project, user, branch, path, revision);
        em.close();
        return ret;
    }

    @PUT
    @Path("/branches/{branch}/resources/data")
    public void putResourceData(@PathParam("project") String project,
                                @PathParam("user") String user,
                                @PathParam("branch") String branch,
                                @QueryParam("path") String path,
                                @DefaultValue("false") @QueryParam("directory") boolean directory,
                                byte[] data) throws ServerException, IOException  {

        EntityManager em = server.getEntityManagerFactory().createEntityManager();
        if (directory) {
            server.mkdir(project, user, branch, path);
        }
        else {
            server.putResourceData(em, project, user, branch, path, data);
        }
        em.close();
    }

    @POST
    @Path("/branches/{branch}/reset")
    public void reset(@PathParam("project") String project,
                                @PathParam("user") String user,
                                @PathParam("branch") String branch,
                                @DefaultValue("mixed") @QueryParam("mode") String mode,
                                @QueryParam("target") String target) throws ServerException, IOException  {
        EntityManager em = server.getEntityManagerFactory().createEntityManager();
        server.reset(em, project, user, branch, mode, target);
        em.close();
    }

    @GET
    @Path("/branches/{branch}/log")
    public Log logBranch(@PathParam("project") String project,
                              @PathParam("user") String user,
                              @PathParam("branch") String branch,
                              @QueryParam("max_count") int maxCount) throws ServerException, IOException {
        EntityManager em = server.getEntityManagerFactory().createEntityManager();
        return server.logBranch(em, project, user, branch, maxCount);
    }

    /*
     * Builds
     */

    @POST
    @Path("/branches/{branch}/builds")
    public BuildDesc build(@PathParam("project") String project,
                           @PathParam("user") String user,
                           @PathParam("branch") String branch,
                           @DefaultValue("false") @QueryParam("rebuild") boolean rebuild) throws ServerException {

        return server.build(project, user, branch, rebuild);
    }

    @GET
    @Path("/branches/{branch}/builds")
    public BuildDesc buildsStatus(@PathParam("project") String project,
                                  @PathParam("user") String user,
                                  @PathParam("branch") String branch,
                                  @QueryParam("id") int id) throws ServerException {

        return server.buildStatus(project, user, branch, id);
    }

    @DELETE
    @Path("/branches/{branch}/builds")
    public void cancelBuild(@PathParam("project") String project,
                                 @PathParam("user") String user,
                                 @PathParam("branch") String branch,
                                 @QueryParam("id") int id) throws ServerException {

        server.cancelBuild(project, user, branch, id);
    }

    @GET
    @Path("/branches/{branch}/builds/log")
    public BuildLog buildLog(@PathParam("project") String project,
                              @PathParam("user") String user,
                              @PathParam("branch") String branch,
                              @QueryParam("id") int id) {

        return server.buildLog(project, user, branch, id);
    }

}
