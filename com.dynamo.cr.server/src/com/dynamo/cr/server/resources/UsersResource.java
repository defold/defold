package com.dynamo.cr.server.resources;

import java.util.Set;

import javax.annotation.security.RolesAllowed;
import javax.persistence.EntityManager;
import javax.ws.rs.GET;
import javax.ws.rs.POST;
import javax.ws.rs.PUT;
import javax.ws.rs.Path;
import javax.ws.rs.PathParam;
import javax.ws.rs.WebApplicationException;
import javax.ws.rs.core.Response;

import com.dynamo.cr.protocol.proto.Protocol.RegisterUser;
import com.dynamo.cr.protocol.proto.Protocol.UserInfo;
import com.dynamo.cr.protocol.proto.Protocol.UserInfoList;
import com.dynamo.cr.server.ServerException;
import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.model.User.Role;

@Path("/users")
@RolesAllowed(value = { "user" })
public class UsersResource extends BaseResource {

    static UserInfo createUserInfo(User u) {
        UserInfo.Builder b = UserInfo.newBuilder();
        b.setId(u.getId())
         .setEmail(u.getEmail())
         .setFirstName(u.getFirstName())
         .setLastName(u.getLastName());
        return b.build();
    }

    @GET
    @Path("/{user}")
    public UserInfo getUserInfo(@PathParam("user") String user) throws ServerException {
        EntityManager em = server.getEntityManagerFactory().createEntityManager();
        try {
            User me = ModelUtil.findUserByEmail(em, securityContext.getUserPrincipal().getName());
            if (me == null) {
                throw new WebApplicationException(Response.Status.NOT_FOUND);
            }

            User u = ModelUtil.findUserByEmail(em, user);
            if (u == null) {
                throw new WebApplicationException(Response.Status.NOT_FOUND);
            }

            // Make sure the user is connected to me. Otherwise forbidden operation
            // We don't have a role an explicit role for this authorization
            // Admin is ok
            if (me.getRole() != Role.ADMIN) {
                // My user info is ok
                if (!u.equals(me)) {
                    if (!u.getConnections().contains(me)) {
                        throw new WebApplicationException(Response.Status.FORBIDDEN);
                    }
                }
            }

            return createUserInfo(u);
        }
        finally {
            em.close();
        }
    }

    @PUT
    @Path("/connect/{user1}/{user2}")
    @RolesAllowed(value = { "admin" })
    public void connect(@PathParam("user1") String user1, @PathParam("user2") String user2) throws ServerException {
        EntityManager em = server.getEntityManagerFactory().createEntityManager();
        try {
            User u1 = server.getUser(em, user1);
            User u2 = server.getUser(em, user2);

            em.getTransaction().begin();
            u1.getConnections().add(u2);
            u2.getConnections().add(u1);
            em.persist(u1);
            em.persist(u2);
            em.getTransaction().commit();
        }
        finally {
            em.close();
        }
    }

    @GET
    @Path("/{user}/connections")
    @RolesAllowed(value = { "self" })
    public UserInfoList getConnections(@PathParam("user") String user) throws ServerException {
        EntityManager em = server.getEntityManagerFactory().createEntityManager();
        try
        {
            User u = server.getUser(em, user);

            Set<User> connections = u.getConnections();
            UserInfoList.Builder b = UserInfoList.newBuilder();
            for (User connection : connections) {
                b.addUsers(createUserInfo(connection));
            }

            return b.build();
        }
        finally {
            em.close();
        }
    }

    @POST
    @Path("/")
    @RolesAllowed(value = { "admin" })
    public UserInfo registerUser(RegisterUser registerUser) throws ServerException {
        EntityManager em = server.getEntityManagerFactory().createEntityManager();

        try {
            em.getTransaction().begin();
            User user = new User();
            user.setEmail(registerUser.getEmail());
            user.setFirstName(registerUser.getFirstName());
            user.setLastName(registerUser.getLastName());
            user.setPassword(registerUser.getPassword());
            em.persist(user);
            em.getTransaction().commit();

            UserInfo userInfo = createUserInfo(user);
            return userInfo;
        }
        finally {
            em.close();
        }
    }

}

