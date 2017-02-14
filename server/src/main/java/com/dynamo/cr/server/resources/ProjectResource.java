package com.dynamo.cr.server.resources;

import com.dynamo.cr.protocol.proto.Protocol.Log;
import com.dynamo.cr.protocol.proto.Protocol.ProjectInfo;
import com.dynamo.cr.server.Server;
import com.dynamo.cr.server.ServerException;
import com.dynamo.cr.server.git.archive.ArchiveCache;
import com.dynamo.cr.server.git.archive.GitArchiveProvider;
import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.Project;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.resources.mappers.ProjectMapper;
import com.dynamo.cr.server.services.ProjectService;
import com.dynamo.cr.server.services.UserService;
import com.dynamo.inject.persist.Transactional;
import com.sun.jersey.api.NotFoundException;
import org.apache.commons.configuration.ConfigurationException;
import org.apache.commons.configuration.plist.XMLPropertyListConfiguration;
import org.apache.commons.io.FileUtils;
import org.apache.commons.io.IOUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import javax.annotation.security.RolesAllowed;
import javax.inject.Inject;
import javax.ws.rs.*;
import javax.ws.rs.core.*;
import javax.ws.rs.core.Response.Status;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.net.URI;
import java.util.Enumeration;
import java.util.Objects;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

@Path("/projects/{owner}/{project}")
@RolesAllowed(value = {"member"})
public class ProjectResource extends BaseResource {

    protected static Logger logger = LoggerFactory.getLogger(ProjectResource.class);

    @Inject
    private ArchiveCache cachedArchives;

    @Inject
    private UserService userService;

    @Inject
    private ProjectService projectService;

    @HEAD
    @Path("/archive/{version}")
    @RolesAllowed(value = {"member"})
    public Response getArchiveHead(@PathParam("project") String project,
                                   @PathParam("version") String version) throws IOException {

        String repository = getRepositoryString(project);
        String sha1 = GitArchiveProvider.getSHA1ForName(repository, version);

        if (sha1 == null) {
            return Response.status(Status.NOT_FOUND).build();
        } else {
            return Response.ok()
                    .header("ETag", sha1)
                    .build();
        }
    }

    @GET
    @Path("/archive")
    @RolesAllowed(value = {"member"})
    public Response getArchive(@PathParam("project") String project,
                               @HeaderParam("If-None-Match") String ifNoneMatch) throws IOException {
        return getArchive(project, "HEAD", ifNoneMatch);
    }

    @GET
    @Path("/archive/{version: .+}")
    @RolesAllowed(value = {"member"})
    public Response getArchive(@PathParam("project") String project,
                               @PathParam("version") String version,
                               @HeaderParam("If-None-Match") String ifNoneMatch) throws IOException {

        String repositoryName = getRepositoryString(project);
        String sha1 = GitArchiveProvider.getSHA1ForName(repositoryName, version);
        if (sha1 == null) {
            return Response.status(Status.NOT_FOUND).build();
        }

        if (ifNoneMatch != null && sha1.equals(ifNoneMatch)) {
            return Response.status(Status.NOT_MODIFIED).build();
        }

        String filePathname = cachedArchives.get(sha1, repositoryName, version);
        final File zipFile = new File(filePathname);

        StreamingOutput output = os -> {
            FileUtils.copyFile(zipFile, os);
            os.close();
        };

        return Response.ok(output, MediaType.APPLICATION_OCTET_STREAM_TYPE)
                .header("content-disposition", String.format("attachment; filename = archive_%s_%s", project, version))
                .header("ETag", sha1)
                .build();
    }

    private String getRepositoryString(String project) {
        return String.format("%s/%s", server.getConfiguration().getRepositoryRoot(), project);
    }

    @POST
    @Path("/members")
    @RolesAllowed(value = {"member"})
    @Transactional
    public void addMember(@PathParam("project") Long projectId, String memberEmail) {
        projectService.addMember(projectId, getUser(), memberEmail);
    }

    @DELETE
    @Path("/members/{id}")
    @Transactional
    public void removeMember(@PathParam("project") Long projectId,
                             @PathParam("id") Long memberId) {
        // Ensure user is valid
        User user = getUser();
        User member = userService.find(memberId)
                .orElseThrow(() -> new ServerException(String.format("No such user %s", memberId), Status.NOT_FOUND));

        Project project = projectService.find(projectId)
                .orElseThrow(() -> new ServerException(String.format("No such project %s", projectId)));

        if (!project.getMembers().contains(member)) {
            throw new NotFoundException(String.format("User %s is not a member of project %s", user.getId(), projectId));
        }
        // Only owners can remove other users and only non-owners can remove themselves
        boolean isOwner = Objects.equals(user.getId(), project.getOwner().getId());
        boolean removeSelf = Objects.equals(user.getId(), member.getId());
        if ((isOwner && removeSelf) || (!isOwner && !removeSelf)) {
            throw new WebApplicationException(403);
        }

        ModelUtil.removeMember(project, member);
        em.persist(project);
        em.persist(member);
    }

    @GET
    @Path("/project_info")
    public ProjectInfo getProjectInfo(@PathParam("project") Long projectId) {
        User user = getUser();
        Project project = projectService.find(projectId)
                .orElseThrow(() -> new ServerException(String.format("No such project %s", projectId)));

        return ProjectMapper.map(server.getConfiguration(), user, project);
    }

    @PUT
    @RolesAllowed(value = {"owner"})
    @Transactional
    @Path("/project_info")
    public void updateProjectInfo(@PathParam("project") Long projectId, ProjectInfo projectInfo) {
        Project project = projectService.find(projectId)
                .orElseThrow(() -> new ServerException(String.format("No such project %s", projectId)));

        if (projectInfo.hasName()) {
            project.setName(projectInfo.getName());
        }

        if (projectInfo.hasDescription()) {
            project.setDescription(projectInfo.getDescription());
        }
    }

    @POST
    @RolesAllowed(value = {"owner"})
    @Transactional
    @Path("/change_owner")
    public void changeOwner(@PathParam("project") Long projectId, @QueryParam("newOwnerId") Long newOwnerId) {

        if (newOwnerId == null) {
            throw new ServerException("Query parameter newOwnerId is missing.", Status.BAD_REQUEST);
        }

        Project project = projectService.find(projectId)
                .orElseThrow(() -> new ServerException(String.format("No such project %s", projectId)));

        if (!changeProjectOwner(project, newOwnerId)) {
            throw new ServerException("Could not change ownership: new owner must be a project member.", Status.FORBIDDEN);
        }

    }

    private boolean changeProjectOwner(Project project, long newOwnerId) {
        User newOwner = userService.find(newOwnerId)
                .orElseThrow(() -> new ServerException(String.format("No such user %s", newOwnerId), Status.NOT_FOUND));
        for (User member : project.getMembers()) {
            if (Objects.equals(member.getId(), newOwner.getId())) {
                logger.debug(String.format("Changing project %d ownership to user %s", project.getId(), newOwnerId));
                project.setOwner(newOwner);
                return true;
            }
        }
        return false;
    }

    @GET
    @Path("/log")
    public Log log(@PathParam("project") String project, @QueryParam("max_count") int maxCount) throws Exception {
        return server.log(em, project, maxCount);
    }

    @DELETE
    @RolesAllowed(value = {"owner"})
    @Transactional
    public void deleteProject(@PathParam("project") Long projectId) {
        Project project = projectService.find(projectId)
                .orElseThrow(() -> new ServerException(String.format("No such project %s", projectId)));
        try {
            ModelUtil.removeProject(em, project);
            ResourceUtil.deleteProjectRepo(project, server.getConfiguration());
        } catch (IOException e) {
            throw new ServerException(String.format("Could not delete git repo for project %s", project.getName()), Status.INTERNAL_SERVER_ERROR);
        }
    }

    /*
     * Defold Engine
     */
    @POST
    @Consumes(MediaType.APPLICATION_OCTET_STREAM)
    @RolesAllowed(value = {"member"})
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

    private XMLPropertyListConfiguration readPlistFromBundle(final String path) throws IOException {
        try (ZipFile file = new ZipFile(path)) {
            final Enumeration<? extends ZipEntry> entries = file.entries();
            while (entries.hasMoreElements()) {
                final ZipEntry entry = entries.nextElement();
                final String entryName = entry.getName();
                if (!entryName.endsWith("Info.plist"))
                    continue;

                try {
                    XMLPropertyListConfiguration plist = new XMLPropertyListConfiguration();
                    plist.load(file.getInputStream(entry));
                    return plist;
                } catch (ConfigurationException e) {
                    throw new IOException("Failed to read Info.plist", e);
                }
            }

            // Info.plist not found
            throw new FileNotFoundException(String.format("Bundle %s didn't contain Info.plist", path));
        }
    }

    @GET
    @RolesAllowed(value = {"anonymous"})
    @Path("/engine/{platform}/{key}.ipa")
    public Response downloadEngine(@PathParam("project") Long projectId,
                                   @PathParam("key") String key,
                                   @PathParam("platform") String platform) throws IOException {

        if (!platform.equals("ios")) {
            // Only iOS for now
            throwWebApplicationException(Status.NOT_FOUND, "Unsupported platform");
        }

        Project project = projectService.find(projectId)
                .orElseThrow(() -> new ServerException(String.format("No such project %s", projectId)));

        String actualKey = Server.getEngineDownloadKey(project);

        if (!key.equals(actualKey)) {
            throwWebApplicationException(Status.FORBIDDEN, "Forbidden");
        }

        final File file = Server.getEngineFile(server.getConfiguration(), String.valueOf(projectId), platform);

        StreamingOutput output = os -> {
            FileUtils.copyFile(file, os);
            os.close();
        };

        return Response.ok(output, MediaType.APPLICATION_OCTET_STREAM)
                .header("content-length", Long.toString(file.length()))
                .header("content-disposition", String.format("attachment; filename=\"%s.ipa\"", key))
                .build();
    }

    @GET
    @RolesAllowed(value = {"anonymous"})
    @Produces({"text/xml"})
    @Path("/engine_manifest/{platform}/{key}")
    public String downloadEngineManifest(@PathParam("owner") String owner,
                                         @PathParam("project") Long projectId,
                                         @PathParam("key") String key,
                                         @PathParam("platform") String platform,
                                         @Context UriInfo uriInfo) throws IOException {

        if (!platform.equals("ios")) {
            // Only iOS for now
            throwWebApplicationException(Status.NOT_FOUND, "Unsupported platform");
        }

        Project project = projectService.find(projectId)
                .orElseThrow(() -> new ServerException(String.format("No such project %s", projectId)));
        String actualKey = Server.getEngineDownloadKey(project);

        if (!key.equals(actualKey)) {
            throwWebApplicationException(Status.FORBIDDEN, "Forbidden");
        }

        InputStream stream = this.getClass().getResourceAsStream("/ota_manifest.plist");
        String manifest = IOUtils.toString(stream);
        stream.close();

        URI engineUri = uriInfo.getBaseUriBuilder().path("projects").path(owner).path(String.valueOf(projectId)).path("engine").path(platform).path(key).build();

        final String ipaPath = Server.getEngineFilePath(server.getConfiguration(), String.valueOf(projectId), platform);

        String bundleid;
        try {
            final XMLPropertyListConfiguration bundleplist = readPlistFromBundle(ipaPath);
            bundleid = bundleplist.getString("CFBundleIdentifier");
        } catch (Exception e) {
            throw new ServerException("Failed to read plist", e, Status.INTERNAL_SERVER_ERROR);
        }

        String manifestPrim = manifest.replace("${URL}", engineUri.toString() + ".ipa");
        manifestPrim = manifestPrim.replace("${BUNDLEID}", bundleid);
        return manifestPrim;
    }
}
