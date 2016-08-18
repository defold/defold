package com.dynamo.cr.server.resources;

import com.dynamo.cr.protocol.proto.Protocol.InvitationAccountInfo;
import com.dynamo.cr.protocol.proto.Protocol.RegisterUser;
import com.dynamo.cr.protocol.proto.Protocol.UserInfo;
import com.dynamo.cr.protocol.proto.Protocol.UserInfoList;
import com.dynamo.cr.server.ServerException;
import com.dynamo.cr.server.model.*;
import com.dynamo.cr.server.services.UserService;
import com.dynamo.inject.persist.Transactional;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import javax.annotation.security.RolesAllowed;
import javax.inject.Inject;
import javax.ws.rs.*;
import javax.ws.rs.core.Response;
import javax.ws.rs.core.Response.Status;
import java.io.IOException;
import java.util.Objects;
import java.util.Set;
import java.util.stream.Collectors;

@Path("/users")
@RolesAllowed(value = {"user"})
public class UsersResource extends BaseResource {

    private static final Logger LOGGER = LoggerFactory.getLogger(UsersResource.class);

    @Inject
    private UserService userService;

    private static UserInfo createUserInfo(User u) {
        UserInfo.Builder b = UserInfo.newBuilder();
        b.setId(u.getId())
                .setEmail(u.getEmail())
                .setFirstName(u.getFirstName())
                .setLastName(u.getLastName());
        return b.build();
    }

    private static InvitationAccountInfo createInvitationAccountInfo(InvitationAccount a) {
        InvitationAccountInfo.Builder b = InvitationAccountInfo.newBuilder();
        b.setOriginalCount(a.getOriginalCount())
                .setCurrentCount(a.getCurrentCount());
        return b.build();
    }

    @GET
    @Path("/{email}")
    public UserInfo getUserInfo(@PathParam("email") String email) {
        User u = ModelUtil.findUserByEmail(em, email);
        if (u == null) {
            throw new WebApplicationException(Response.Status.NOT_FOUND);
        }

        return createUserInfo(u);
    }

    @PUT
    @Path("/{user}/connections/{user2}")
    @Transactional
    public void connect(@PathParam("user2") Long user2) {
        User u = getUser();
        User u2 = userService.find(user2)
                .orElseThrow(() -> new ServerException(String.format("No such user %s", user2), Status.NOT_FOUND));

        if (Objects.equals(u.getId(), u2.getId())) {
            throw new ServerException("A user can not be connected to him/herself.", Response.Status.FORBIDDEN);
        }

        u.getConnections().add(u2);
        em.persist(u);
    }

    @GET
    @Path("/{user}/connections")
    public UserInfoList getConnections() {
        User u = getUser();

        Set<User> connections = u.getConnections();
        UserInfoList.Builder b = UserInfoList.newBuilder();
        for (User connection : connections) {
            b.addUsers(createUserInfo(connection));
        }

        return b.build();
    }

    @GET
    @Path("/{user}/remove")
    @Transactional
    public Response remove(@PathParam("user") Long userId) {
        User user = getUser();

        if (!user.getId().equals(userId)) {
            throw new ServerException("Deleting other users's accounts is not allowed.", Response.Status.FORBIDDEN);
        }

        Set<Project> ownedProjects = getOwnedProjects(user);
        Set<Project> ownedProjectsWithOtherMembers = getProjectsWithOtherMembers(ownedProjects);

        if (!ownedProjectsWithOtherMembers.isEmpty()) {

            String projectNames = ownedProjectsWithOtherMembers.stream()
                    .map(Project::getName)
                    .collect(Collectors.joining(", "));

            throw new ServerException(
                    String.format("User owns projects with other members. The members needs to be deleted from the " +
                            "project or the project ownership needs to be transferred to another user. Projects " +
                            "affected: %s", projectNames),
                    Response.Status.FORBIDDEN);
        }

        deleteProjects(ownedProjects);

        LOGGER.info(String.format("Deleting user with ID %s", userId));
        userService.remove(user);

        return okResponse("User %s deleted", userId);
    }

    private Set<Project> getProjectsWithOtherMembers(Set<Project> ownedProjects) {
        return ownedProjects.stream().filter(project -> project.getMembers().size() > 1).collect(Collectors.toSet());
    }

    private Set<Project> getOwnedProjects(User user) {
        return user.getProjects().stream().filter(project -> project.getOwner().equals(user)).collect(Collectors.toSet());
    }

    private void deleteProjects(Set<Project> projects) {
        for (Project project : projects) {
            ModelUtil.removeProject(em, project);

            try {
                ResourceUtil.deleteProjectRepo(project, server.getConfiguration());
            } catch (IOException e) {
                throw new ServerException(String.format("Could not delete git repo for project %s", project.getName()), Status.INTERNAL_SERVER_ERROR);
            }
        }
    }

    @POST
    @RolesAllowed(value = {"admin"})
    @Transactional
    public UserInfo registerUser(RegisterUser registerUser) {
        /*
         * NOTE: Old registration method as admin role
         * OpenID registration is the only supported method. We should
         * probably remove this and related tests soon
         */

        User user = new User();
        user.setEmail(registerUser.getEmail());
        user.setFirstName(registerUser.getFirstName());
        user.setLastName(registerUser.getLastName());
        user.setPassword(registerUser.getPassword());
        em.persist(user);
        em.flush();

        return createUserInfo(user);
    }

    @PUT
    @Path("/{user}/invite/{email}")
    @Transactional
    public Response invite(@PathParam("user") String user, @PathParam("email") String email) {
        InvitationAccount a = userService.getInvitationAccount(user);
        if (a.getCurrentCount() == 0) {
            throwWebApplicationException(Status.FORBIDDEN, "Inviter has no invitations left");
        }
        a.setCurrentCount(a.getCurrentCount() - 1);
        em.persist(a);

        User u = getUser();
        String inviter = String.format("%s %s", u.getFirstName(), u.getLastName());
        userService.invite(email, inviter, u.getEmail(), a.getOriginalCount());

        return okResponse("User %s invited", email);
    }

    @GET
    @Path("/{user}/invitation_account")
    @Transactional
    public InvitationAccountInfo getInvitationAccount(@PathParam("user") String user) {
        InvitationAccount a = userService.getInvitationAccount(user);
        return createInvitationAccountInfo(a);
    }
}

