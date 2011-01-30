package com.dynamo.cr.server.resources;

import javax.annotation.security.RolesAllowed;
import javax.persistence.EntityManager;
import javax.ws.rs.GET;
import javax.ws.rs.Path;
import javax.ws.rs.PathParam;

import com.dynamo.cr.protocol.proto.Protocol.UserInfo;
import com.dynamo.cr.server.ServerException;
import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.User;

@Path("/users")
// TOOD: CHANGE ROLE TO SELF AND ADMIN?
// We don't wan't anyone to info for arbitrary user..!
@RolesAllowed(value = { "user" })
public class UsersResource extends BaseResource {

    @GET
    @Path("/{user}")
    public UserInfo getUserInfo(@PathParam("user") String user) throws ServerException {
        EntityManager em = server.getEntityManagerFactory().createEntityManager();
        User u = ModelUtil.findUserByEmail(em, user);
        if (u == null) {
            throw new ServerException(String.format("No such user: %s", user));
        }
        UserInfo.Builder b = UserInfo.newBuilder();
        b.setId(u.getId())
         .setEmail(u.getEmail())
         .setFirstName(u.getFirstName())
         .setLastName(u.getLastName());
        return b.build();
    }
}

