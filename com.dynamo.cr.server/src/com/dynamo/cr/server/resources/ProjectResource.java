package com.dynamo.cr.server.resources;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.List;

import javax.ws.rs.DELETE;
import javax.ws.rs.DefaultValue;
import javax.ws.rs.GET;
import javax.ws.rs.POST;
import javax.ws.rs.PUT;
import javax.ws.rs.Path;
import javax.ws.rs.PathParam;
import javax.ws.rs.QueryParam;

import com.dynamo.cr.proto.Config.Application;
import com.dynamo.cr.proto.Config.ProjectConfiguration;
import com.dynamo.cr.protocol.proto.Protocol;
import com.dynamo.cr.protocol.proto.Protocol.ApplicationInfo;
import com.dynamo.cr.protocol.proto.Protocol.BranchList;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus;
import com.dynamo.cr.protocol.proto.Protocol.BuildDesc;
import com.dynamo.cr.protocol.proto.Protocol.BuildLog;
import com.dynamo.cr.protocol.proto.Protocol.ResolveStage;
import com.dynamo.cr.protocol.proto.Protocol.ResourceInfo;
import com.dynamo.cr.server.ServerException;
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

@Path("/")
public class ProjectResource extends BaseResource {

    /*
     * Project
     */

    @GET
    @Path("/{project}")
    public ProjectConfiguration getProjectConfiguration(@PathParam("project") String project) {
        return server.getProjectConfiguration(project);
    }

    @GET
    @Path("/{project}/application_data")
    public byte[] getApplicationData(@PathParam("project") String project,
                                     @QueryParam("platform") String platform) throws IOException {
        ProjectConfiguration config = server.getProjectConfiguration(project);

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
    @Path("/{project}/application_info")
    public ApplicationInfo getApplicationInfo(@PathParam("project") String project,
                                              @QueryParam("platform") String platform) throws IOException {
        ProjectConfiguration config = server.getProjectConfiguration(project);
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

    /*
     * Branch
     */

    @GET
    @Path("/{project}/{user}")
    public BranchList getBranchList(@PathParam("project") String project,
                                @PathParam("user") String user) {
        String[] branch_names = server.getBranchNames(project, user);
        Protocol.BranchList.Builder b = Protocol.BranchList.newBuilder();
        for (String branch : branch_names) {
            b.addBranches(branch);
        }
        return b.build();
    }

    @GET
    @Path("/{project}/{user}/{branch}")
    public BranchStatus getBranchStatus(@PathParam("project") String project,
                                          @PathParam("user") String user,
                                          @PathParam("branch") String branch) throws IOException, ServerException {
        return server.getBranchStatus(project, user, branch);
    }

    @PUT
    @Path("/{project}/{user}/{branch}")
    public void createBranch(@PathParam("project") String project,
                             @PathParam("user") String user,
                             @PathParam("branch") String branch) throws IOException, ServerException {
        server.createBranch(project, user, branch);
    }

    @DELETE
    @Path("/{project}/{user}/{branch}")
    public void deleteBranch(@PathParam("project") String project,
                             @PathParam("user") String user,
                             @PathParam("branch") String branch) throws ServerException {
        server.deleteBranch(project, user, branch);
    }

    @POST
    @Path("/{project}/{user}/{branch}/update")
    public BranchStatus updateBranch(@PathParam("project") String project,
                               @PathParam("user") String user,
                               @PathParam("branch") String branch) throws IOException, ServerException {

        server.updateBranch(project, user, branch);
        return server.getBranchStatus(project, user, branch);
    }

    @POST
    @Path("/{project}/{user}/{branch}/commit")
    public void commitBranch(@PathParam("project") String project,
                             @PathParam("user") String user,
                             @PathParam("branch") String branch,
                             @QueryParam("all") boolean all,
                             String message) throws IOException, ServerException {

        if (all)
            server.commitBranch(project, user, branch, message);
        else
            server.commitMergeBranch(project, user, branch, message);
    }

    @POST
    @Path("/{project}/{user}/{branch}/publish")
    public void publishBranch(@PathParam("project") String project,
                              @PathParam("user") String user,
                              @PathParam("branch") String branch) throws IOException, ServerException {

        server.publishBranch(project, user, branch);
    }

    @POST
    @Path("/{project}/{user}/{branch}/resolve")
    public void resolve(@PathParam("project") String project,
                        @PathParam("user") String user,
                        @PathParam("branch") String branch,
                        @QueryParam("path") String path,
                        @QueryParam("stage") String stage) throws IOException, ServerException {

        ResolveStage s;
        if (stage.equals("base"))
            s = ResolveStage.BASE;
        else if (stage.equals("yours"))
            s = ResolveStage.YOURS;
        else if (stage.equals("theirs"))
            s = ResolveStage.THEIRS;
        else
            throw new ServerException(String.format("Unknown stage: %s", stage));

        server.resolveResource(project, user, branch, path, s);
    }

    /*
     * Resource
     */

    @GET
    @Path("/{project}/{user}/{branch}/resources/info")
    public ResourceInfo getResourceInfo(@PathParam("project") String project,
                                          @PathParam("user") String user,
                                          @PathParam("branch") String branch,
                                          @QueryParam("path") String path) throws IOException, ServerException {

        return server.getResourceInfo(project, user, branch, path);
    }

    @DELETE
    @Path("/{project}/{user}/{branch}/resources/info")
    public void deleteResource(@PathParam("project") String project,
                               @PathParam("user") String user,
                               @PathParam("branch") String branch,
                               @QueryParam("path") String path) throws IOException, ServerException {

        server.deleteResource(project, user, branch, path);
    }

    @POST
    @Path("/{project}/{user}/{branch}/resources/rename")
    public void renameResource(@PathParam("project") String project,
                               @PathParam("user") String user,
                               @PathParam("branch") String branch,
                               @QueryParam("source") String source,
                               @QueryParam("destination") String destination) throws IOException, ServerException {

        server.renameResource(project, user, branch, source, destination);
    }

    @PUT
    @Path("/{project}/{user}/{branch}/resources/revert")
    public void revertResource(@PathParam("project") String project,
                               @PathParam("user") String user,
                               @PathParam("branch") String branch,
                               @QueryParam("path") String path) throws IOException, ServerException {

        server.revertResource(project, user, branch, path);
    }

    @GET
    @Path("/{project}/{user}/{branch}/resources/data")
    public byte[] getResourceData(@PathParam("project") String project,
                                  @PathParam("user") String user,
                                  @PathParam("branch") String branch,
                                  @QueryParam("path") String path) throws IOException, ServerException {

        return server.getResourceData(project, user, branch, path);
    }

    @PUT
    @Path("/{project}/{user}/{branch}/resources/data")
    public void putResourceData(@PathParam("project") String project,
                                @PathParam("user") String user,
                                @PathParam("branch") String branch,
                                @QueryParam("path") String path,
                                @DefaultValue("false") @QueryParam("directory") boolean directory,
                                byte[] data) throws ServerException, IOException  {

        if (directory) {
            server.mkdir(project, user, branch, path);
        }
        else {
            server.putResourceData(project, user, branch, path, data);
        }
    }

    /*
     * Builds
     */

    @POST
    @Path("/{project}/{user}/{branch}/builds")
    public BuildDesc build(@PathParam("project") String project,
                           @PathParam("user") String user,
                           @PathParam("branch") String branch,
                           @DefaultValue("false") @QueryParam("rebuild") boolean rebuild) {

        return server.build(project, user, branch, rebuild);
    }

    @GET
    @Path("/{project}/{user}/{branch}/builds")
    public BuildDesc buildsStatus(@PathParam("project") String project,
                                  @PathParam("user") String user,
                                  @PathParam("branch") String branch,
                                  @QueryParam("id") int id) {

        return server.buildStatus(project, user, branch, id);
    }

    @GET
    @Path("/{project}/{user}/{branch}/builds/log")
    public BuildLog buildLog(@PathParam("project") String project,
                              @PathParam("user") String user,
                              @PathParam("branch") String branch,
                              @QueryParam("id") int id) {

        return server.buildLog(project, user, branch, id);
    }

}
