package com.dynamo.cr.server.resources;

import java.util.Set;

import javax.annotation.security.RolesAllowed;
import javax.persistence.EntityManager;
import javax.ws.rs.GET;
import javax.ws.rs.POST;
import javax.ws.rs.Path;
import javax.ws.rs.PathParam;
import javax.ws.rs.WebApplicationException;

import com.dynamo.cr.protocol.proto.Protocol.RegisterUser;
import com.dynamo.cr.protocol.proto.Protocol.UserInfo;
import com.dynamo.cr.protocol.proto.Protocol.UserInfoList;
import com.dynamo.cr.server.ServerException;
import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.User;

@Path("/users")
// TOOD: CHANGE ROLE TO SELF AND ADMIN?
// We don't wan't anyone to info for arbitrary user..!
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
        User u = ModelUtil.findUserByEmail(em, user);
        if (u == null) {
            throw new WebApplicationException(404);
        }

        return createUserInfo(u);
    }

    @GET
    @Path("/{user}/connections")
    public UserInfoList getConnections(@PathParam("user") String user) throws ServerException {
        EntityManager em = server.getEntityManagerFactory().createEntityManager();
        User u = server.getUser(em, user);

        Set<User> connections = u.getConnections();
        UserInfoList.Builder b = UserInfoList.newBuilder();
        for (User connection : connections) {
            b.addUsers(createUserInfo(connection));
        }

        return b.build();
    }

    @POST
    @Path("/")
    @RolesAllowed(value = { "admin" })
    public UserInfo registerUser(RegisterUser registerUser) throws ServerException {
        EntityManager em = server.getEntityManagerFactory().createEntityManager();

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

}

