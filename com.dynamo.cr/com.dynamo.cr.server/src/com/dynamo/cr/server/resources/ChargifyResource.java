package com.dynamo.cr.server.resources;

import javax.annotation.security.RolesAllowed;
import javax.ws.rs.FormParam;
import javax.ws.rs.HeaderParam;
import javax.ws.rs.POST;
import javax.ws.rs.Path;
import javax.ws.rs.core.Response.Status;

import com.dynamo.cr.server.util.ChargifyUtil;

@Path("/chargify")
@RolesAllowed(value = { "anonymous" })
public class ChargifyResource extends BaseResource {

    @POST
    @Path("")
    public void handleWebHook(@HeaderParam(ChargifyUtil.SIGNATURE_HEADER_NAME) String signature,
            @FormParam("event") String event, @FormParam("payload[site]") String site,
            @FormParam("payload[subscription]") String subscription, String body) {
        String key = server.getConfiguration().getBillingSharedKey();
        String expectedSignature = new String(ChargifyUtil.generateSignature(key.getBytes(), body.getBytes()));
        if (!expectedSignature.equals(signature)) {
            throwWebApplicationException(Status.FORBIDDEN, "Not authorized");
        }
        // TODO: Add webhooks with tests
    }

}

