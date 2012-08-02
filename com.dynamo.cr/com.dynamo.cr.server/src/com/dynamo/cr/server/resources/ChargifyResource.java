package com.dynamo.cr.server.resources;

import javax.annotation.security.RolesAllowed;
import javax.ws.rs.FormParam;
import javax.ws.rs.HeaderParam;
import javax.ws.rs.POST;
import javax.ws.rs.Path;
import javax.ws.rs.core.Response.Status;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.slf4j.Marker;
import org.slf4j.MarkerFactory;

import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.UserSubscription;
import com.dynamo.cr.server.model.UserSubscription.State;
import com.dynamo.cr.server.util.ChargifyUtil;

@Path("/chargify")
@RolesAllowed(value = { "anonymous" })
public class ChargifyResource extends BaseResource {

    private static Logger logger = LoggerFactory.getLogger(ChargifyResource.class);
    private static final Marker BILLING_MARKER = MarkerFactory.getMarker("BILLING");

    @POST
    public void handleWebHook(@HeaderParam(ChargifyUtil.SIGNATURE_HEADER_NAME) String signature,
            @FormParam("event") String event, @FormParam("payload[subscription][id]") String subscriptionId,
            @FormParam("payload[subscription][state]") String state,
            @FormParam("payload[subscription][previous_state]") String previousState,
            @FormParam("payload[subscription][cancellation_message]") String cancellationMessage, String body) {
        String key = server.getConfiguration().getBillingSharedKey();
        String expectedSignature = ChargifyUtil.generateSignature(key, body);
        if (!expectedSignature.equals(signature)) {
            logger.warn(BILLING_MARKER,
                    "Unauthorized attempt to access chargify resource, body: " + body);
            throwWebApplicationException(Status.FORBIDDEN, "Not authorized");
        }
        if (event != null) {
            if (event.equals(ChargifyUtil.SIGNUP_SUCCESS_WH)) {
                handleSignupSuccess(subscriptionId);
            } else if (event.equals(ChargifyUtil.SIGNUP_FAILURE_WH)) {
                handleSignupFailure(subscriptionId, cancellationMessage);
            } else if (event.equals(ChargifyUtil.RENEWAL_FAILURE_WH)) {
                handleRenewalFailure(subscriptionId, cancellationMessage);
            } else if (event.equals(ChargifyUtil.SUBSCRIPTION_STATE_CHANGE_WH)) {
                handleSubscriptionStateChange(subscriptionId, state, previousState);
            }
            // Silently ignore other webhooks
        }
    }

    private void handleSignupSuccess(String subscriptionId) {
        logger.info(BILLING_MARKER, "Subscription %s was successfully started.", subscriptionId);
        UserSubscription s = ModelUtil.findUserSubscriptionByExternalId(em, subscriptionId);
        s.setState(State.ACTIVE);
        em.getTransaction().begin();
        em.persist(s);
        em.getTransaction().commit();
    }

    private void cancelSubscription(String subscriptionId, String cancellationMessage) {
        UserSubscription s = ModelUtil.findUserSubscriptionByExternalId(em, subscriptionId);
        s.setState(State.CANCELED);
        s.setCancellationMessage(cancellationMessage);
        em.getTransaction().begin();
        em.persist(s);
        em.getTransaction().commit();
    }

    private void handleSignupFailure(String subscriptionId, String cancellationMessage) {
        logger.info(BILLING_MARKER, "Subscription %s failed to start because: %s", subscriptionId,
                cancellationMessage);
        cancelSubscription(subscriptionId, cancellationMessage);
    }

    private void handleRenewalFailure(String subscriptionId, String cancellationMessage) {
        logger.info(BILLING_MARKER, "Subscription %s failed to renew because: %s", subscriptionId,
                cancellationMessage);
        cancelSubscription(subscriptionId, cancellationMessage);
    }

    private void handleSubscriptionStateChange(String subscriptionId, String state, String previousState) {
        logger.info(BILLING_MARKER,
                String.format("Subscription %s changed from %s to %s.", subscriptionId, previousState, state));
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

