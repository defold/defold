package com.dynamo.cr.server.resources;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.URI;
import java.util.Collection;
import java.util.Enumeration;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
import java.util.zip.ZipOutputStream;

import javax.annotation.security.RolesAllowed;
import javax.ws.rs.Consumes;
import javax.ws.rs.DELETE;
import javax.ws.rs.GET;
import javax.ws.rs.HEAD;
import javax.ws.rs.HeaderParam;
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

import org.apache.commons.configuration.ConfigurationException;
import org.apache.commons.configuration.plist.XMLPropertyListConfiguration;
import org.apache.commons.io.FileUtils;
import org.apache.commons.io.IOUtils;
import org.apache.commons.io.filefilter.AbstractFileFilter;
import org.apache.commons.io.filefilter.IOFileFilter;
import org.apache.commons.io.filefilter.TrueFileFilter;
import org.eclipse.jgit.api.CloneCommand;
import org.eclipse.jgit.api.Git;
import org.eclipse.jgit.api.errors.GitAPIException;
import org.eclipse.jgit.api.errors.InvalidRefNameException;
import org.eclipse.jgit.api.errors.JGitInternalException;
import org.eclipse.jgit.api.errors.RefAlreadyExistsException;
import org.eclipse.jgit.api.errors.RefNotFoundException;
import org.eclipse.jgit.lib.ObjectId;
import org.eclipse.jgit.lib.Ref;
import org.eclipse.jgit.lib.Repository;
import org.eclipse.jgit.revwalk.RevCommit;
import org.eclipse.jgit.revwalk.RevObject;
import org.eclipse.jgit.revwalk.RevTag;
import org.eclipse.jgit.revwalk.RevWalk;
import org.eclipse.jgit.storage.file.FileRepositoryBuilder;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.proto.Config.Configuration;
import com.dynamo.cr.protocol.proto.Protocol.Log;
import com.dynamo.cr.protocol.proto.Protocol.ProjectInfo;
import com.dynamo.cr.server.Server;
import com.dynamo.cr.server.ServerException;
import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.Project;
import com.dynamo.cr.server.model.User;
import com.dynamo.inject.persist.Transactional;
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
    private static void zipFiles(File directory, File zipFile, String comment) throws IOException {
        ZipOutputStream zipOut = null;

        try {
            zipOut = new ZipOutputStream(new FileOutputStream(zipFile));
            zipOut.setComment(comment);
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

    static String getSHA1ForName(String repository, String name) throws IOException {

        Repository repo = null;
        RevWalk walk = null;

        try {
            FileRepositoryBuilder builder = new FileRepositoryBuilder();
            repo = builder.setGitDir(new File(repository))
                    .findGitDir()
                    .build();

            walk = new RevWalk(repo);
            Ref p = repo.getRef(name);
            if (p == null) {
                // if not ref
                ObjectId objId = repo.resolve(name);
                if (objId != null)
                    return objId.getName();
                return null;
            }
            RevObject object = walk.parseAny(p.getObjectId());
            if (object instanceof RevTag) {
                RevTag tag = (RevTag) object;
                return tag.getObject().getName();
            } else if (object instanceof RevCommit) {
                RevCommit commit = (RevCommit) object;
                return commit.getName();
            } else {
                throw new IllegalArgumentException(String.format("Unknown object: %s", object.toString()));
            }
        } finally {
            if (walk != null) {
                walk.dispose();
            }
            if (repo != null) {
                repo.close();
            }
        }
    }

    @HEAD
    @Path("/archive/{version}")
    @RolesAllowed(value = { "member" })
    public Response getArchiveHead(@PathParam("project") String project,
                                      @PathParam("version") String version) throws IOException {

        // TODO: Refactor this to a method and replace
        String repository = String.format("%s/%s", server.getConfiguration().getRepositoryRoot(), project);
        String sha1 = getSHA1ForName(repository, version);
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
    @RolesAllowed(value = { "member" })
    public Response getArchive(@PathParam("project") String project,
                               @HeaderParam("If-None-Match") String ifNoneMatch) throws IOException {
        return getArchive(project, "HEAD", ifNoneMatch);
    }

    @GET
    @Path("/archive/{version}")
    @RolesAllowed(value = { "member" })
    public Response getArchive(@PathParam("project") String project,
                               @PathParam("version") String version,
                               @HeaderParam("If-None-Match") String ifNoneMatch) throws IOException {

        // NOTE: Currently neither caching of created zip-files nor appropriate cache-headers (ETag)
        // Appropriate ETag might be SHA1 of zip-file or even SHA1 from git (head commit)

        String repository = String.format("%s/%s", server.getConfiguration().getRepositoryRoot(), project);
        String sha1 = getSHA1ForName(repository, version);
        if (sha1 == null) {
            return Response.status(Status.NOT_FOUND).build();
        }

        if (ifNoneMatch != null && sha1.equals(ifNoneMatch)) {
            return Response.status(Status.NOT_MODIFIED).build();
        }

        File cloneTo = null;
        final File zipFile = File.createTempFile("archive", ".zip");

        try {
            cloneTo = Files.createTempDir();
            // NOTE: No shallow clone support in current JGit
            CloneCommand clone = Git.cloneRepository()
                    .setURI(repository)
                    .setBare(false)
                    .setDirectory(cloneTo);

            try {
                Git git = clone.call();
                git.checkout().setName(version).call();
            } catch (JGitInternalException|RefAlreadyExistsException e) {
                throw new ServerException("Failed to checkout repo", e, Status.INTERNAL_SERVER_ERROR);
            } catch (RefNotFoundException e) {
                throw new ServerException(String.format("Version not found: %s", version), e, Status.INTERNAL_SERVER_ERROR);
            } catch (InvalidRefNameException e) {
                throw new ServerException(String.format("Version not found: %s", version), e, Status.INTERNAL_SERVER_ERROR);
            } catch (GitAPIException e) {
                throw new ServerException("Failed to checkout repo", e, Status.INTERNAL_SERVER_ERROR);
            }

            zipFiles(cloneTo, zipFile, sha1);
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
                .header("ETag", sha1)
                .build();
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
        return ResourceUtil.createProjectInfo(server.getConfiguration(), user, project);
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
                              @QueryParam("max_count") int maxCount) throws Exception {
        Log log = server.log(em, project, maxCount);
        return log;
    }

    @DELETE
    @RolesAllowed(value = { "owner" })
    @Transactional
    public void deleteProject(@PathParam("project") String projectId)  {
        Project project = server.getProject(em, projectId);
        // Delete git repo
        Configuration configuration = server.getConfiguration();
        String repositoryRoot = configuration.getRepositoryRoot();
        File projectPath = new File(String.format("%s/%d", repositoryRoot, project.getId()));
        try {
            ModelUtil.removeProject(em, project);
            FileUtils.deleteDirectory(projectPath);
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


    private XMLPropertyListConfiguration readPlistFromBundle(final String path) throws IOException {
        final ZipFile file = new ZipFile( path );
        try
        {
            final Enumeration<? extends ZipEntry> entries = file.entries();
            while ( entries.hasMoreElements() )
            {
                final ZipEntry entry = entries.nextElement();
                final String entryName = entry.getName();
                if( !entryName.endsWith("Info.plist") )
                    continue;

                try {
                    XMLPropertyListConfiguration plist = new XMLPropertyListConfiguration();
                    plist.load(file.getInputStream( entry ));
                    return plist;
                } catch (ConfigurationException e) {
                    throw new IOException("Failed to read Info.plist", e);
                }
            }

            // Info.plist not found
            throw new FileNotFoundException(String.format("Bundle %s didn't contain Info.plist", path));
        }
        finally
        {
            file.close();
        }
    }

    @GET
    @RolesAllowed(value = { "anonymous" })
    @Path("/engine/{platform}/{key}.ipa")
    public Response downloadEngine(@PathParam("project") String projectId,
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

        final File file = Server.getEngineFile(server.getConfiguration(), projectId, platform);

        StreamingOutput output = new StreamingOutput() {
            @Override
            public void write(OutputStream os) throws IOException, WebApplicationException {
                FileUtils.copyFile(file, os);
                os.close();
            }
        };

        return Response.ok(output, MediaType.APPLICATION_OCTET_STREAM)
                .header("content-length", Long.toString(file.length()))
                .header("content-disposition", String.format("attachment; filename=\"%s.ipa\"", key))
                .build();
    }

    @GET
    @RolesAllowed(value = { "anonymous" })
    @Produces({"text/xml"})
    @Path("/engine_manifest/{platform}/{key}")
    public String downloadEngineManifest(@PathParam("owner") String owner,
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

        InputStream stream = this.getClass().getResourceAsStream("/ota_manifest.plist");
        String manifest = IOUtils.toString(stream);
        stream.close();

        URI engineUri = uriInfo.getBaseUriBuilder().path("projects").path(owner).path(projectId).path("engine").path(platform).path(key).build();

        final String ipaPath = Server.getEngineFilePath(server.getConfiguration(), projectId, platform);

        String bundleid = "unknown";
        try {
            final XMLPropertyListConfiguration bundleplist = readPlistFromBundle( ipaPath );
            bundleid = bundleplist.getString("CFBundleIdentifier");
        } catch (Exception e) {
            throw new ServerException("Failed to read plist", e, Status.INTERNAL_SERVER_ERROR);
        }

        String manifestPrim = manifest.replace("${URL}", engineUri.toString()+".ipa");
        manifestPrim = manifestPrim.replace("${BUNDLEID}", bundleid);
        return manifestPrim;
    }

}
