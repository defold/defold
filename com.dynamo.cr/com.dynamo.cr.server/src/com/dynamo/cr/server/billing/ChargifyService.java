package com.dynamo.cr.server.billing;

import java.io.IOException;
import java.net.URI;
import java.util.Iterator;

import javax.ws.rs.WebApplicationException;
import javax.ws.rs.core.MediaType;
import javax.ws.rs.core.Response;
import javax.ws.rs.core.Response.Status;
import javax.ws.rs.core.UriBuilder;

import org.codehaus.jackson.JsonNode;
import org.codehaus.jackson.map.ObjectMapper;
import org.codehaus.jackson.node.ObjectNode;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.slf4j.Marker;
import org.slf4j.MarkerFactory;

import com.dynamo.cr.proto.Config.BillingProduct;
import com.dynamo.cr.proto.Config.Configuration;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.model.UserSubscription;
import com.dynamo.cr.server.model.UserSubscription.CreditCard;
import com.dynamo.cr.server.model.UserSubscription.State;
import com.google.inject.Inject;
import com.sun.jersey.api.client.Client;
import com.sun.jersey.api.client.ClientResponse;
import com.sun.jersey.api.client.WebResource;
import com.sun.jersey.api.client.config.ClientConfig;
import com.sun.jersey.api.client.config.DefaultClientConfig;
import com.sun.jersey.api.client.filter.HTTPBasicAuthFilter;

public class ChargifyService implements IBillingProvider {

    private static Logger logger = LoggerFactory.getLogger(ChargifyService.class);
    private static final Marker BILLING_MARKER = MarkerFactory.getMarker("BILLING");

    private WebResource chargifyResource;
    private ObjectMapper mapper;

    @Inject
    public ChargifyService(Configuration configuration) {
        ClientConfig clientConfig = new DefaultClientConfig();

        Client client = Client.create(clientConfig);

        client.addFilter(new HTTPBasicAuthFilter(configuration.getBillingApiKey(), "x"));
        URI uri = UriBuilder.fromUri(configuration.getBillingApiUrl()).build();
        chargifyResource = client.resource(uri);
        mapper = new ObjectMapper();
    }

    public WebResource getChargifyResource() {
        return this.chargifyResource;
    }

    private void verifyAndLogResponse(ClientResponse response, String subject,
            String operation) throws WebApplicationException {
        int status = response.getStatus();
        if ((status / 100) == 2) {
            logger.info(BILLING_MARKER, "{} was {}.", subject, operation);
        } else {
            ObjectMapper mapper = new ObjectMapper();
            String msg = null;
            String logMsg = null;
            try {
                JsonNode root = mapper.readTree(response.getEntityInputStream());
                JsonNode errorsNode = root.get("errors");
                if (errorsNode != null) {
                    Iterator<JsonNode> errors = errorsNode.getElements();
                    StringBuilder builder = new StringBuilder();
                    StringBuilder logBuilder = new StringBuilder();
                    logBuilder.append('{');
                    while (errors.hasNext()) {
                        JsonNode error = errors.next();
                        builder.append(error.getTextValue());
                        logBuilder.append('\"').append(error.getTextValue()).append('\"');
                        if (errors.hasNext()) {
                            builder.append(" ");
                            logBuilder.append(", ");
                        }
                    }
                    logBuilder.append('}');
                    msg = builder.toString();
                    logMsg = logBuilder.toString();
                } else {
                    logger.error(BILLING_MARKER, "Erroneous response ({}) containing no errors: '{}'", status,
                            root.toString());
                }
            } catch (IOException e) {
                msg = "An external system error ocurred.";
                logMsg = "Could not read billing provider response: " + e.getMessage();
            }
            logger.error(BILLING_MARKER, "{} could not be {}, status: {}", new Object[] { subject, operation, status });
            logger.error(BILLING_MARKER, logMsg);
            Response r = Response
                    .status(status)
                    .type(MediaType.TEXT_PLAIN)
                    .entity(msg)
                    .build();
            throw new WebApplicationException(r);
        }
    }

    private void verifyAndLogResponse(ClientResponse response, UserSubscription subscription,
            String operation) throws WebApplicationException {
        String subject = String.format("Subscription %d (external: %d)", subscription.getId(),
                subscription.getExternalId());
        verifyAndLogResponse(response, subject, operation);
    }

    @Override
    public UserSubscription signUpUser(BillingProduct product, User user, String creditCard, String ccExpirationMonth,
            String ccExpirationYear, String cvv)
            throws WebApplicationException {
        ObjectNode root = this.mapper.createObjectNode();
        ObjectNode customer = this.mapper.createObjectNode();
        // We can't populate with custom reference here (Defold-id), since
        // chargify customers can't be deleted and reference must be unique.
        customer.put("first_name", user.getFirstName());
        customer.put("last_name", user.getLastName());
        customer.put("email", user.getEmail());
        root.put("customer", customer);
        ClientResponse response = chargifyResource.path("/customers.json").type(MediaType.APPLICATION_JSON_TYPE)
                .accept(MediaType.APPLICATION_JSON_TYPE).post(ClientResponse.class, root.toString());
        verifyAndLogResponse(response,
                String.format("User %s %s (%d)", user.getFirstName(), user.getLastName(), user.getId()), "registered");

        JsonNode c;
        try {
            c = mapper.readTree(response.getEntityInputStream());
        } catch (IOException e) {
            logger.error(BILLING_MARKER, "Customer could not be parsed: {}", e.getMessage());
            throw new WebApplicationException(e, Status.INTERNAL_SERVER_ERROR);
        }
        int customerId = c.get("customer").get("id").getIntValue();

        root = this.mapper.createObjectNode();
        ObjectNode subscription = this.mapper.createObjectNode();
        subscription.put("product_handle", product.getHandle());
        subscription.put("customer_id", customerId);
        ObjectNode cc = this.mapper.createObjectNode();
        cc.put("full_number", creditCard);
        cc.put("expiration_month", ccExpirationMonth);
        cc.put("expiration_year", ccExpirationYear);
        cc.put("cvv", cvv);
        subscription.put("credit_card_attributes", cc);
        root.put("subscription", subscription);
        response = chargifyResource.path("/subscriptions.json").type(MediaType.APPLICATION_JSON_TYPE)
                .accept(MediaType.APPLICATION_JSON_TYPE).post(ClientResponse.class, root.toString());
        verifyAndLogResponse(
                response,
                String.format("Subscription by %s %s (%d) to product %s", user.getFirstName(), user.getLastName(),
                        user.getId(), product.getHandle()), "registered");
        UserSubscription s = new UserSubscription();
        jsonToUserSubscription(response, s);
        return s;
    }

    @Override
    public void reactivateSubscription(UserSubscription subscription) throws WebApplicationException {
        ClientResponse response = chargifyResource
                .path(String.format("/subscriptions/%d/reactivate.json", subscription.getExternalId()))
                .type(MediaType.APPLICATION_JSON_TYPE).accept(MediaType.APPLICATION_JSON_TYPE)
                .put(ClientResponse.class, "");
        verifyAndLogResponse(response, subscription, "reactivated");
        jsonToUserSubscription(response, subscription);
    }

    @Override
    public void migrateSubscription(UserSubscription subscription, BillingProduct product)
            throws WebApplicationException {
        ObjectNode root = mapper.createObjectNode();
        ObjectNode migration = mapper.createObjectNode();
        migration.put("product_id", product.getId());
        root.put("migration", migration);
        // Only for debug purposes
        logger.warn(BILLING_MARKER, "Migration input");
        logger.warn(BILLING_MARKER, root.toString());
        ClientResponse response = chargifyResource
                .path(String.format("/subscriptions/%d/migrations.json", subscription.getExternalId()))
                .type(MediaType.APPLICATION_JSON_TYPE).accept(MediaType.APPLICATION_JSON_TYPE)
                .post(ClientResponse.class, root.toString());
        verifyAndLogResponse(response, subscription,
                String.format("migrated from %d to %d", subscription.getProductId(), product.getId()));
    }

    @Override
    public void cancelSubscription(UserSubscription subscription) throws WebApplicationException {
        ClientResponse response = chargifyResource
                .path(String.format("/subscriptions/%d.json", subscription.getExternalId()))
                .type(MediaType.APPLICATION_JSON_TYPE).accept(MediaType.APPLICATION_JSON_TYPE)
                .delete(ClientResponse.class);
        verifyAndLogResponse(response, subscription, "canceled");
    }

    @Override
    public UserSubscription getSubscription(Long subscriptionId) throws WebApplicationException {
        ClientResponse response = chargifyResource
                .path(String.format("/subscriptions/%d.json", subscriptionId))
                .type(MediaType.APPLICATION_JSON_TYPE).accept(MediaType.APPLICATION_JSON_TYPE)
                .get(ClientResponse.class);
        int status = response.getStatus();
        if (status >= 200 && status < 300) {
            UserSubscription subscription = new UserSubscription();
            jsonToUserSubscription(response, subscription);
            logger.info(BILLING_MARKER, "Subscription (external: {}) was created.", subscription.getExternalId());
            return subscription;
        } else {
            logger.error(BILLING_MARKER, "Subscription {} could not be found: {}", subscriptionId, status);
        }
        return null;
    }

    private void jsonToUserSubscription(ClientResponse response, UserSubscription subscription) {
        JsonNode root;
        try {
            root = this.mapper.readTree(response.getEntityInputStream());
        } catch (IOException e) {
            logger.error(BILLING_MARKER, "Subscription could not be parsed: {}", e.getMessage());
            throw new WebApplicationException(e, Status.INTERNAL_SERVER_ERROR);
        }
        JsonNode subJson = root.get("subscription");
        int id = subJson.get("id").asInt();
        CreditCard cc = new CreditCard();
        JsonNode ccJson = subJson.get("credit_card");
        cc.setMaskedNumber(ccJson.get("masked_card_number").getTextValue());
        cc.setExpirationMonth(ccJson.get("expiration_month").getIntValue());
        cc.setExpirationYear(ccJson.get("expiration_year").getIntValue());
        subscription.setCreditCard(cc);
        subscription.setExternalId((long) id);
        subscription.setExternalCustomerId((long) ccJson.get("customer_id").getIntValue());
        subscription.setProductId((long) subJson.get("product").get("id").getIntValue());
        String state = subJson.get("state").getTextValue();
        if (state.equals("active")) {
            subscription.setState(State.ACTIVE);
        } else if (state.equals("canceled")) {
            subscription.setState(State.CANCELED);
        } else {
            subscription.setState(State.PENDING);
        }
    }
}
