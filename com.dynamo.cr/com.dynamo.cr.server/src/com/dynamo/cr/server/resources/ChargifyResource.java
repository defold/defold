package com.dynamo.cr.server.resources;

import javax.annotation.security.RolesAllowed;
import javax.ws.rs.FormParam;
import javax.ws.rs.HeaderParam;
import javax.ws.rs.POST;
import javax.ws.rs.Path;
import javax.ws.rs.core.Response.Status;

import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.UserSubscription;
import com.dynamo.cr.server.model.UserSubscription.State;
import com.dynamo.cr.server.util.ChargifyUtil;

@Path("/chargify")
@RolesAllowed(value = { "anonymous" })
public class ChargifyResource extends BaseResource {

    @POST
    @Path("")
    public void handleWebHook(@HeaderParam(ChargifyUtil.SIGNATURE_HEADER_NAME) String signature,
            @FormParam("event") String event, @FormParam("payload[subscription][id]") String subscriptionId,
            @FormParam("payload[subscription][state]") String state, String body) {
        String key = server.getConfiguration().getBillingSharedKey();
        String expectedSignature = new String(ChargifyUtil.generateSignature(key.getBytes(), body.getBytes()));
        if (!expectedSignature.equals(signature)) {
            throwWebApplicationException(Status.FORBIDDEN, "Not authorized");
        }
        if (event != null) {
            if (event.equals(ChargifyUtil.SIGNUP_SUCCESS_WH)) {
                handleSignupSuccess(subscriptionId);
            } else if (event.equals(ChargifyUtil.SIGNUP_FAILURE_WH)) {
                handleSignupFailure(subscriptionId);
            } else if (event.equals(ChargifyUtil.RENEWAL_FAILURE_WH)) {
                handleRenewalFailure(subscriptionId);
            } else if (event.equals(ChargifyUtil.SUBSCRIPTION_STATE_CHANGE_WH)) {
                handleSubscriptionStateChange(subscriptionId, state);
            } else {
                throwWebApplicationException(Status.BAD_REQUEST, "Unknown webhook");
            }
        }
    }

    private void handleSignupSuccess(String subscriptionId) {
        UserSubscription s = ModelUtil.findUserSubscriptionByExternalId(em, subscriptionId);
        s.setState(State.ACTIVE);
        em.getTransaction().begin();
        em.persist(s);
        em.getTransaction().commit();
    }

    private void cancelSubscription(String subscriptionId) {
        UserSubscription s = ModelUtil.findUserSubscriptionByExternalId(em, subscriptionId);
        s.setState(State.CANCELED);
        em.getTransaction().begin();
        em.persist(s);
        em.getTransaction().commit();
    }

    private void handleSignupFailure(String subscriptionId) {
        cancelSubscription(subscriptionId);
    }

    private void handleRenewalFailure(String subscriptionId) {
        cancelSubscription(subscriptionId);
    }

    private void handleSubscriptionStateChange(String subscriptionId, String state) {
        State s = null;
        if (state.equals("active")) {
            s = State.ACTIVE;
        } else if (state.equals("canceled")) {
            s = State.CANCELED;
        }
        if (s != null) {
            UserSubscription us = ModelUtil.findUserSubscriptionByExternalId(em, subscriptionId);
            us.setState(s);
            em.getTransaction().begin();
            em.persist(us);
            em.getTransaction().commit();
        }
    }
}

