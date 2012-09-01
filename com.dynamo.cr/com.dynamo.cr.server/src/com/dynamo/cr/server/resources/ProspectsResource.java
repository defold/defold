package com.dynamo.cr.server.resources;

import java.util.List;

import javax.persistence.TypedQuery;
import javax.ws.rs.PUT;
import javax.ws.rs.Path;
import javax.ws.rs.PathParam;
import javax.ws.rs.core.Response;
import javax.ws.rs.core.Response.Status;

import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.Prospect;
import com.dynamo.inject.persist.Transactional;

@Path("/prospects")
public class ProspectsResource extends BaseResource {

    @PUT
    @Path("/{email}")
    @Transactional
    public Response newProspect(@PathParam("email") String email) {
        if (server.openRegistration(em)) {
            server.invite(em, email, "Defold", "info@defold.se", server.getConfiguration().getOpenInvitationCount());
            String msg = "An invitation email has been sent to %s. " +
            		     "Instructions and registration link is in the email. Please check the spam folder if no registration email " +
            		     "is found in your inbox.";
            return okResponse(msg, email);
        } else {
            TypedQuery<Prospect> q = em.createQuery("select p from Prospect p where p.email = :email", Prospect.class);
            List<Prospect> lst = q.setParameter("email", email).getResultList();
            if (lst.size() > 0) {
                String msg = String.format("The email %s is already registred. An invitation will be sent to you as soon as we can " +
                		                   "handle the high volume of registrations.", email);
                throwWebApplicationException(Status.CONFLICT, msg);
            }
            Prospect p = new Prospect();
            p.setEmail(email);
            em.persist(p);
            ModelUtil.subscribeToNewsLetter(em, email, "", "");
            em.flush();
            String msg = "The registration is currently closed due to unexpectedly high volume of registrations. " +
            		     "You've been placed in a queue and will be invited as soon as possible.";
            return okResponse(msg);
        }
    }

}
