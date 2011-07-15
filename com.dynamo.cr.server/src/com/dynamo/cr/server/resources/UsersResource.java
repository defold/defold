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
    @Path("/{email}")
    public UserInfo getUserInfo(@PathParam("email") String email) throws ServerException {
        EntityManager em = server.getEntityManagerFactory().createEntityManager();
        try {
            User u = ModelUtil.findUserByEmail(em, email);
            if (u == null) {
                throw new WebApplicationException(Response.Status.NOT_FOUND);
            }

            return createUserInfo(u);
        }
        finally {
            em.close();
        }
    }

    @PUT
    @Path("/{user}/connections/{user2}")
    public void connect(@PathParam("user") String user, @PathParam("user2") String user2) throws ServerException {
        if (user.equals(user2)) {
            throw new ServerException("A user can not be connected to him/herself.", Response.Status.FORBIDDEN);
        }
        EntityManager em = server.getEntityManagerFactory().createEntityManager();
        try {
            User u = server.getUser(em, user);
            User u2 = server.getUser(em, user2);

            em.getTransaction().begin();
            u.getConnections().add(u2);
            em.persist(u);
            em.getTransaction().commit();
        }
        finally {
            em.close();
        }
    }

    @GET
    @Path("/{user}/connections")
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

