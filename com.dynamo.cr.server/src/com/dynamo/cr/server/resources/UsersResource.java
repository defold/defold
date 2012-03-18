package com.dynamo.cr.server.resources;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.UUID;

import javax.annotation.security.RolesAllowed;
import javax.persistence.EntityManager;
import javax.persistence.TypedQuery;
import javax.ws.rs.GET;
import javax.ws.rs.POST;
import javax.ws.rs.PUT;
import javax.ws.rs.Path;
import javax.ws.rs.PathParam;
import javax.ws.rs.WebApplicationException;
import javax.ws.rs.core.Response;
import javax.ws.rs.core.Response.Status;

import com.dynamo.cr.proto.Config.EMailTemplate;
import com.dynamo.cr.protocol.proto.Protocol.RegisterUser;
import com.dynamo.cr.protocol.proto.Protocol.UserInfo;
import com.dynamo.cr.protocol.proto.Protocol.UserInfoList;
import com.dynamo.cr.server.ServerException;
import com.dynamo.cr.server.mail.EMail;
import com.dynamo.cr.server.model.Invitation;
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
        /*
         * NOTE: Old registration method as admin role
         * OpenID registration is the only supported method. We should
         * probably remove this and related tests soon
         */

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

    @PUT
    @Path("/{user}/invite/{email}")
    public Response invite(@PathParam("user") String user, @PathParam("email") String email) throws ServerException {
        EntityManager em = server.getEntityManagerFactory().createEntityManager();
        try {
            TypedQuery<Invitation> q = em.createQuery("select i from Invitation i where i.email = :email", Invitation.class);
            List<Invitation> lst = q.setParameter("email", email).getResultList();
            if (lst.size() > 0) {
                throwWebApplicationException(Status.CONFLICT, "User already invited");
            }

            String key = UUID.randomUUID().toString();
            User u = server.getUser(em, user);

            EMailTemplate template = server.getConfiguration().getInvitationTemplate();
            Map<String, String> params = new HashMap<String, String>();
            params.put("inviter", String.format("%s %s", u.getFirstName(), u.getLastName()));
            params.put("key", key);
            EMail emailMessage = EMail.format(template, email, params);

            em.getTransaction().begin();
            Invitation invitation = new Invitation();
            invitation.setEmail(email);
            invitation.setInviter(u);
            invitation.setRegistrationKey(key);
            em.persist(invitation);
            server.getMailProcessor().send(em, emailMessage);
            em.getTransaction().commit();
            server.getMailProcessor().process();
        }
        finally {
            em.close();
        }

        return okResponse("User %s invited", email);
    }
}

