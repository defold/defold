package com.dynamo.cr.server.resources;

import com.dynamo.cr.protocol.proto.Protocol.*;
import com.dynamo.cr.server.ServerException;
import com.dynamo.cr.server.auth.AccessTokenAuthenticator;
import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.Project;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.services.UserService;
import com.dynamo.inject.persist.Transactional;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import javax.annotation.security.RolesAllowed;
import javax.inject.Inject;
import javax.servlet.http.HttpServletRequest;
import javax.ws.rs.*;
import javax.ws.rs.core.Context;
import javax.ws.rs.core.Response;
import javax.ws.rs.core.Response.Status;
import java.io.IOException;
import java.util.Objects;
import java.util.Set;
import java.util.stream.Collectors;

@Path("/users")
public class UsersResource extends BaseResource {

    private static final Logger LOGGER = LoggerFactory.getLogger(UsersResource.class);

    @Inject
    private UserService userService;

    @Inject
    private AccessTokenAuthenticator accessTokenAuthenticator;

    private static UserInfo createUserInfo(User u) {
        UserInfo.Builder b = UserInfo.newBuilder();
        b.setId(u.getId())
                .setEmail(u.getEmail())
                .setFirstName(u.getFirstName())
                .setLastName(u.getLastName());
        return b.build();
    }

    @POST
    @Path("signup")
    @Transactional
    public void signup(SignupRequest signupRequest) {
        userService.signupEmailPassword(signupRequest.getEmail(), signupRequest.getFirstName(),
                signupRequest.getLastName(), signupRequest.getPassword());
    }

    @GET
    @Path("signup/verify/{token}")
    @Transactional
    public SignupVerificationResponse verify(@PathParam("token") String token, @Context HttpServletRequest request) {
        // Create user from activation token.
        User user = userService.create(token);

        // Login user.
        String accessToken = accessTokenAuthenticator.createSessionToken(user, request.getRemoteAddr());

        return SignupVerificationResponse.newBuilder()
                .setAuth(accessToken)
                .setEmail(user.getEmail())
                .setUserId(user.getId())
                .setFirstName(user.getFirstName())
                .setLastName(user.getLastName())
                .build();
    }

    @PUT
    @Path("{user}/password/change")
    @Transactional
    @RolesAllowed(value = {"user"})
    public void changePassword(@PathParam("user") Long userId,
                               PasswordChangeRequest passwordChangeRequest) {
        userService.changePassword(userId, passwordChangeRequest.getCurrentPassword(), passwordChangeRequest.getNewPassword());
    }

    @POST
    @Path("password/forgot")
    @Transactional
    public void forgotPassword(@QueryParam("email") String email) {
        userService.forgotPassword(email);
    }

    @POST
    @Path("password/reset")
    @Transactional
    public void resetPassword(PasswordResetRequest passwordResetRequest) {
        userService.resetPassword(passwordResetRequest.getEmail(), passwordResetRequest.getToken(),
                passwordResetRequest.getNewPassword());
    }

    @GET
    @Path("/{email}")
    @RolesAllowed(value = {"user"})
    public UserInfo getUserInfo(@PathParam("email") String email) {
        User user = userService.findByEmail(email)
                .orElseThrow(() -> new WebApplicationException(Response.Status.NOT_FOUND));

        return createUserInfo(user);
    }

    @PUT
    @Path("/{user}/connections/{user2}")
    @Transactional
    @RolesAllowed(value = {"user"})
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
    @RolesAllowed(value = {"user"})
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
    @RolesAllowed(value = {"user"})
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
}

