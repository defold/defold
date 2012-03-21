package com.dynamo.cr.server.resources;

import java.util.List;

import javax.persistence.TypedQuery;
import javax.ws.rs.PUT;
import javax.ws.rs.Path;
import javax.ws.rs.PathParam;
import javax.ws.rs.core.Response;
import javax.ws.rs.core.Response.Status;

import com.dynamo.cr.server.model.Prospect;
import com.dynamo.inject.persist.Transactional;

@Path("/prospects")
public class ProspectsResource extends BaseResource {

    @PUT
    @Path("/{email}")
    @Transactional
    public Response newProspect(@PathParam("email") String email) {
        TypedQuery<Prospect> q = em.createQuery("select p from Prospect p where p.email = :email", Prospect.class);
        List<Prospect> lst = q.setParameter("email", email).getResultList();
        if (lst.size() > 0) {
            throwWebApplicationException(Status.CONFLICT, "Email already registered");
        }
        Prospect p = new Prospect();
        p.setEmail(email);
        em.persist(p);
        em.flush();
        return okResponse("Email %s registered", email);
    }

}
