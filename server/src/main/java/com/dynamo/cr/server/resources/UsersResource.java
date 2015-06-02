package com.dynamo.cr.server.resources;

import java.util.Set;

import javax.annotation.security.RolesAllowed;
import javax.ws.rs.GET;
import javax.ws.rs.POST;
import javax.ws.rs.PUT;
import javax.ws.rs.Path;
import javax.ws.rs.PathParam;
import javax.ws.rs.WebApplicationException;
import javax.ws.rs.core.Response;
import javax.ws.rs.core.Response.Status;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.slf4j.Marker;
import org.slf4j.MarkerFactory;

import com.dynamo.cr.protocol.proto.Protocol.InvitationAccountInfo;
import com.dynamo.cr.protocol.proto.Protocol.RegisterUser;
import com.dynamo.cr.protocol.proto.Protocol.UserInfo;
import com.dynamo.cr.protocol.proto.Protocol.UserInfoList;
import com.dynamo.cr.server.ServerException;
import com.dynamo.cr.server.model.InvitationAccount;
import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.User;
import com.dynamo.inject.persist.Transactional;

@Path("/users")
@RolesAllowed(value = { "user" })
public class UsersResource extends BaseResource {

    private static Logger logger = LoggerFactory.getLogger(UsersResource.class);
    private static final Marker BILLING_MARKER = MarkerFactory.getMarker("BILLING");

    static UserInfo createUserInfo(User u) {
        UserInfo.Builder b = UserInfo.newBuilder();
        b.setId(u.getId())
         .setEmail(u.getEmail())
         .setFirstName(u.getFirstName())
         .setLastName(u.getLastName());
        return b.build();
    }

    static InvitationAccountInfo createInvitationAccountInfo(InvitationAccount a) {
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
    public void connect(@PathParam("user2") String user2) {
        User u = getUser();
        User u2 = server.getUser(em, user2);
        if (u.getId() == u2.getId()) {
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

    @POST
    @RolesAllowed(value = { "admin" })
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

        UserInfo userInfo = createUserInfo(user);
        return userInfo;
    }

    @PUT
    @Path("/{user}/invite/{email}")
    @Transactional
    public Response invite(@PathParam("user") String user, @PathParam("email") String email) {
        InvitationAccount a = server.getInvitationAccount(em, user);
        if (a.getCurrentCount() == 0) {
            throwWebApplicationException(Status.FORBIDDEN, "Inviter has no invitations left");
        }
        a.setCurrentCount(a.getCurrentCount() - 1);
        em.persist(a);

        User u = getUser();
        String inviter = String.format("%s %s", u.getFirstName(), u.getLastName());
        server.invite(em, email, inviter, u.getEmail(), a.getOriginalCount());

        return okResponse("User %s invited", email);
    }

    @GET
    @Path("/{user}/invitation_account")
    @Transactional
    public InvitationAccountInfo getInvitationAccount(@PathParam("user") String user) {
        InvitationAccount a = server.getInvitationAccount(em, user);
        return createInvitationAccountInfo(a);
    }

}

