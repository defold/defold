package com.dynamo.cr.server.resources;

import com.dynamo.cr.protocol.proto.Protocol;
import com.dynamo.cr.protocol.proto.Protocol.Log;
import com.dynamo.cr.protocol.proto.Protocol.ProjectInfo;
import com.dynamo.cr.server.Server;
import com.dynamo.cr.server.ServerException;
import com.dynamo.cr.server.git.archive.ArchiveCache;
import com.dynamo.cr.server.git.archive.GitArchiveProvider;
import com.dynamo.cr.server.managers.ProjectManager;
import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.Project;
import com.dynamo.cr.server.model.User;
import com.dynamo.inject.persist.Transactional;
import org.apache.commons.configuration.ConfigurationException;
import org.apache.commons.configuration.plist.XMLPropertyListConfiguration;
import org.apache.commons.io.FileUtils;
import org.apache.commons.io.IOUtils;
import org.eclipse.jgit.api.Git;
import org.eclipse.jgit.revwalk.RevCommit;
import org.joda.time.DateTime;
import org.joda.time.format.DateTimeFormat;
import org.joda.time.format.DateTimeFormatter;
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
    private ProjectManager projectManager;

    private ArchiveCache cachedArchives;

    @Inject
    public void setArchiveCache(ArchiveCache cachedArchives) {
        this.cachedArchives = cachedArchives;
    }

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
        projectManager.addMember(projectId, memberEmail, getUser());
    }

    @DELETE
    @Path("/members/{id}")
    @Transactional
    public void removeMember(@PathParam("project") String projectId, @PathParam("id") String memberId) {
        User administrator = getUser();
        User member = server.getUser(em, memberId);
        projectManager.removeMember(Long.valueOf(projectId), member, administrator);
    }

    @GET
    @Path("/project_info")
    public ProjectInfo getProjectInfo(@PathParam("project") Long projectId) {
        User user = getUser();
        Project project = projectManager.getProject(projectId)
                .orElseThrow(() -> new ServerException("Project not found.", Status.NOT_FOUND));
        return ResourceUtil.createProjectInfo(server.getConfiguration(), user, project);
    }

    @PUT
    @RolesAllowed(value = {"owner"})
    @Transactional
    @Path("/project_info")
    public void updateProjectInfo(@PathParam("project") String projectId, ProjectInfo projectInfo) {
        Project project = projectManager.getProject(Long.valueOf(projectId))
                .orElseThrow(() -> new ServerException("Project not found.", Status.NOT_FOUND));
        project.setName(projectInfo.getName());
        project.setDescription(projectInfo.getDescription());
    }

    @POST
    @RolesAllowed(value = {"owner"})
    @Transactional
    @Path("/change_owner")
    public void changeOwner(@PathParam("project") Long projectId, @QueryParam("newOwnerId") Long newOwnerId) {

        if (newOwnerId == null) {
            throw new ServerException("Query parameter newOwnerId is missing.", Status.BAD_REQUEST);
        }

        Project project = projectManager.getProject(projectId)
                .orElseThrow(() -> new ServerException("Project not found.", Status.NOT_FOUND));

        if (!changeProjectOwner(project, newOwnerId)) {
            throw new ServerException("Could not change ownership: new owner must be a project member.", Status.FORBIDDEN);
        }
    }

    private boolean changeProjectOwner(Project project, long newOwnerId) {
        User newOwner = server.getUser(em, Long.toString(newOwnerId));
        for (User member : project.getMembers()) {
            if (Objects.equals(member.getId(), newOwner.getId())) {
                validateMaxProjectCount(newOwner);
                logger.debug(String.format("Changing project %d ownership to user %s", project.getId(), newOwnerId));
                project.setOwner(newOwner);
                return true;
            }
        }
        return false;
    }

    private void validateMaxProjectCount(User newOwner) throws ServerException {
        long projectCount = ModelUtil.getProjectCount(em, newOwner);
        int maxProjectCount = server.getConfiguration().getMaxProjectCount();

        if (projectCount >= maxProjectCount) {
            throw new ServerException(String.format("Max number of projects (%d) has already been reached.", maxProjectCount),
                    Status.FORBIDDEN);
        }
    }

    @GET
    @Path("/log")
    public Log log(@PathParam("project") String project, @QueryParam("max_count") int maxCount) throws Exception {
        String sourcePath = String.format("%s/%s", server.getConfiguration().getRepositoryRoot(), project);

        DateTimeFormatter formatter = DateTimeFormat.forPattern("yyyy-MM-dd HH:mm:ss Z");
        Log.Builder logBuilder = Log.newBuilder();
        Git git = Git.open(new File(sourcePath));
        Iterable<RevCommit> revLog = git.log().setMaxCount(maxCount).call();
        for (RevCommit revCommit : revLog) {
            Protocol.CommitDesc.Builder commit = Protocol.CommitDesc.newBuilder();
            commit.setName(revCommit.getCommitterIdent().getName());
            commit.setId(revCommit.getId().toString());
            commit.setMessage(revCommit.getShortMessage());
            commit.setEmail(revCommit.getCommitterIdent().getEmailAddress());
            long commitTime = revCommit.getCommitTime();
            commit.setDate(formatter.print(new DateTime(commitTime * 1000)));
            logBuilder.addCommits(commit);
        }

        return logBuilder.build();
    }

    @DELETE
    @RolesAllowed(value = {"owner"})
    @Transactional
    public void deleteProject(@PathParam("project") Long projectId) {
        projectManager.removeProject(projectId, server.getConfiguration().getRepositoryRoot());
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

        Project project = projectManager.getProject(projectId)
                .orElseThrow(() -> new ServerException("Project not found.", Status.NOT_FOUND));

        String actualKey = Server.getEngineDownloadKey(project);

        if (!key.equals(actualKey)) {
            throwWebApplicationException(Status.FORBIDDEN, "Forbidden");
        }

        final File file = Server.getEngineFile(server.getConfiguration(), projectId.toString(), platform);

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

        Project project = projectManager.getProject(projectId)
                .orElseThrow(() -> new ServerException("Project not found.", Status.NOT_FOUND));

        String actualKey = Server.getEngineDownloadKey(project);

        if (!key.equals(actualKey)) {
            throwWebApplicationException(Status.FORBIDDEN, "Forbidden");
        }

        InputStream stream = this.getClass().getResourceAsStream("/ota_manifest.plist");
        String manifest = IOUtils.toString(stream);
        stream.close();

        URI engineUri = uriInfo.getBaseUriBuilder().path("projects").path(owner).path(projectId.toString()).path("engine").path(platform).path(key).build();

        final String ipaPath = Server.getEngineFilePath(server.getConfiguration(), projectId.toString(), platform);

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
