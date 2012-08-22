package com.dynamo.cr.server.resources;

import javax.annotation.security.RolesAllowed;
import javax.ws.rs.FormParam;
import javax.ws.rs.HeaderParam;
import javax.ws.rs.POST;
import javax.ws.rs.Path;
import javax.ws.rs.core.Response;
import javax.ws.rs.core.Response.Status;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.slf4j.Marker;
import org.slf4j.MarkerFactory;

import com.dynamo.cr.proto.Config.BillingProduct;
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
    public Response handleWebHook(@HeaderParam(ChargifyUtil.SIGNATURE_HEADER_NAME) String signature,
            @FormParam("event") String event, @FormParam("payload[subscription][id]") String subscriptionId,
            @FormParam("payload[subscription][state]") String state,
            @FormParam("payload[subscription][previous_state]") String previousState,
            @FormParam("payload[subscription][cancellation_message]") String cancellationMessage,
            @FormParam("payload[subscription][product][handle]") String productHandle,
            @FormParam("payload[previous_product][handle]") String previousProductHandle, String body) {
        String key = server.getConfiguration().getBillingSharedKey();
        String expectedSignature = ChargifyUtil.generateSignature(key, body);
        if (!expectedSignature.equals(signature)) {
            logger.warn(BILLING_MARKER,
                    "Unauthorized attempt to access chargify resource, body: {}", body);
            throwWebApplicationException(Status.FORBIDDEN, "Not authorized");
        }
        if (event != null) {
            if (event.equals(ChargifyUtil.SIGNUP_SUCCESS_WH)) {
                handleSignupSuccess(subscriptionId);
            } else if (event.equals(ChargifyUtil.SIGNUP_FAILURE_WH)) {
                handleSignupFailure(subscriptionId, cancellationMessage);
            } else if (event.equals(ChargifyUtil.SUBSCRIPTION_STATE_CHANGE_WH)) {
                handleSubscriptionStateChange(subscriptionId, state, previousState, cancellationMessage);
            } else if (event.equals(ChargifyUtil.SUBSCRIPTION_PRODUCT_CHANGE_WH)) {
                handleSubscriptionProductChange(subscriptionId, productHandle, previousProductHandle, state);
            } else if (event.equals(ChargifyUtil.TEST_WH)) {
                // Silently ignore test webhooks
            } else {
                // Warn about unknown webhooks
                logger.warn(BILLING_MARKER, "Received unknown webhook {}", event);
            }
        }
        return okResponse("");
    }

    private UserSubscription getUserSubscription(String subscriptionId) {
        UserSubscription s = ModelUtil.findUserSubscriptionByExternalId(em, subscriptionId);
        if (s == null) {
            logger.error(BILLING_MARKER, "Subscription {} could not be found", subscriptionId);
            throwWebApplicationException(Status.NOT_FOUND, "Subscription could not be found");
        }
        return s;
    }

    private void handleSignupSuccess(String subscriptionId) {
        logger.info(BILLING_MARKER, "Subscription {} was successfully started.", subscriptionId);
        UserSubscription s = getUserSubscription(subscriptionId);
        s.setState(State.ACTIVE);
        em.getTransaction().begin();
        em.persist(s);
        em.getTransaction().commit();
    }

    private void handleSignupFailure(String subscriptionId, String cancellationMessage) {
        logger.info(BILLING_MARKER, "Subscription {} failed to start because: {}", subscriptionId,
                cancellationMessage);
        UserSubscription s = getUserSubscription(subscriptionId);
        s.setState(State.CANCELED);
        s.setCancellationMessage(cancellationMessage);
        em.getTransaction().begin();
        em.persist(s);
        em.getTransaction().commit();
    }

    private State getDefoldState(String chargifyState) {
        State s = null;
        if (chargifyState.equals("active") || chargifyState.equals("past_due")) {
            s = State.ACTIVE;
        } else if (chargifyState.equals("canceled")) {
            s = State.CANCELED;
        }
        if (s == null) {
            logger.warn(BILLING_MARKER, "The state \"{}\" could not be parsed.", chargifyState);
        }
        return s;
    }

    private void handleSubscriptionStateChange(String subscriptionId, String state, String previousState,
            String cancellationMessage) {
        logger.info(BILLING_MARKER, "Subscription {} changed from {} to {}.", new Object[] { subscriptionId,
                previousState, state });
        State s = getDefoldState(state);
        if (s != null) {
            UserSubscription us = getUserSubscription(subscriptionId);
            us.setState(s);
            us.setCancellationMessage(cancellationMessage);
            em.getTransaction().begin();
            em.persist(us);
            em.getTransaction().commit();
        }
    }

    private void handleSubscriptionProductChange(String subscriptionId, String productHandle,
            String previousProductHandle, String state) {
        logger.info(BILLING_MARKER, "Subscription {} migrated from {} to {}.", new Object[] { subscriptionId,
                previousProductHandle, productHandle });
        UserSubscription us = getUserSubscription(subscriptionId);
        BillingProduct p = server.getProductByHandle(productHandle);
        us.setProductId((long) p.getId());
        State s = getDefoldState(state);
        if (s != null) {
            us.setState(s);
        }
        em.getTransaction().begin();
        em.persist(us);
        em.getTransaction().commit();
    }

}

