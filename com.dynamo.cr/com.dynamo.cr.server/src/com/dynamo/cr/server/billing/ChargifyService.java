package com.dynamo.cr.server.billing;

import java.io.IOException;
import java.net.URI;
import java.util.Iterator;

import javax.ws.rs.WebApplicationException;
import javax.ws.rs.core.MediaType;
import javax.ws.rs.core.Response;
import javax.ws.rs.core.UriBuilder;

import org.codehaus.jackson.JsonNode;
import org.codehaus.jackson.map.ObjectMapper;
import org.codehaus.jackson.node.ObjectNode;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.slf4j.Marker;
import org.slf4j.MarkerFactory;

import com.dynamo.cr.proto.Config.Configuration;
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

    private static final String BASE_PATH = "/subscriptions";

    private WebResource chargifyResource;
    private ObjectMapper mapper;

    @Inject
    public ChargifyService(Configuration configuration) {
        ClientConfig clientConfig = new DefaultClientConfig();

        Client client = Client.create(clientConfig);

        client.addFilter(new HTTPBasicAuthFilter(configuration.getBillingApiKey(), "x"));
        URI uri = UriBuilder.fromUri(String.format("%s%s", configuration.getBillingApiUrl(), BASE_PATH)).build();
        chargifyResource = client.resource(uri);
        mapper = new ObjectMapper();
    }

    public WebResource getChargifyResource() {
        return this.chargifyResource;
    }

    private void verifyAndLogResponse(ClientResponse response, UserSubscription subscription,
            String operation) throws WebApplicationException {
        int status = response.getStatus();
        String prefix = String.format("Subscription %d (external: %d)", subscription.getId(),
                subscription.getExternalId());
        if (status >= 200 && status < 300) {
            logger.info(BILLING_MARKER, String.format("%s was %s.", prefix, operation));
        } else {
            ObjectMapper mapper = new ObjectMapper();
            String msg = null;
            try {
                JsonNode root = mapper.readTree(response.getEntityInputStream());
                Iterator<JsonNode> errors = root.get("errors").getElements();
                StringBuilder builder = new StringBuilder();
                while (errors.hasNext()) {
                    JsonNode error = errors.next();
                    builder.append(error.getTextValue()).append(".");
                    if (errors.hasNext()) {
                        builder.append(" ");
                    }
                }
                msg = builder.toString();
            } catch (IOException e) {
                msg = "Could not read billing provider response: " + e.getMessage();
            }
            logger.error(BILLING_MARKER, String.format("%s could not be %s.", prefix, operation));
            logger.error(BILLING_MARKER, msg);
            Response r = Response
                    .status(status)
                    .type(MediaType.TEXT_PLAIN)
                    .entity(msg)
                    .build();
            throw new WebApplicationException(r);
        }
    }

    @Override
    public void reactivateSubscription(UserSubscription subscription) throws WebApplicationException {
        ClientResponse response = chargifyResource
                .path(String.format("/%d/reactivate.json", subscription.getExternalId()))
                .type(MediaType.APPLICATION_JSON_TYPE).accept(MediaType.APPLICATION_JSON_TYPE)
                .put(ClientResponse.class, "");
        verifyAndLogResponse(response, subscription, "reactivated");
        jsonToUserSubscription(response, subscription);
    }

    @Override
    public void migrateSubscription(UserSubscription subscription, int newProductId) throws WebApplicationException {
        ObjectNode root = mapper.createObjectNode();
        ObjectNode migration = mapper.createObjectNode();
        migration.put("product_id", newProductId);
        root.put("migration", migration);
        ClientResponse response = chargifyResource
                .path(String.format("/%d/migrations.json", subscription.getExternalId()))
                .type(MediaType.APPLICATION_JSON_TYPE).accept(MediaType.APPLICATION_JSON_TYPE)
                .post(ClientResponse.class, root.toString());
        verifyAndLogResponse(response, subscription,
                String.format("migrated from %d to %d", subscription.getProductId(), newProductId));
    }

    @Override
    public void cancelSubscription(UserSubscription subscription) throws WebApplicationException {
        ClientResponse response = chargifyResource
                .path(String.format("/%d.json", subscription.getExternalId()))
                .type(MediaType.APPLICATION_JSON_TYPE).accept(MediaType.APPLICATION_JSON_TYPE)
                .delete(ClientResponse.class);
        verifyAndLogResponse(response, subscription, "canceled");
    }

    @Override
    public UserSubscription getSubscription(Long subscriptionId) throws WebApplicationException {
        ClientResponse response = chargifyResource
                .path(String.format("/%d.json", subscriptionId))
                .type(MediaType.APPLICATION_JSON_TYPE).accept(MediaType.APPLICATION_JSON_TYPE)
                .get(ClientResponse.class);
        int status = response.getStatus();
        if (status >= 200 && status < 300) {
            UserSubscription subscription = new UserSubscription();
            jsonToUserSubscription(response, subscription);
            logger.info(String.format("Subscription (external: %d) was created.", subscription.getExternalId()));
            return subscription;
        } else {
            logger.error(BILLING_MARKER,
                    String.format("Subscription %d could not be found: %d", subscriptionId, status));
        }
        return null;
    }

    private void jsonToUserSubscription(ClientResponse response, UserSubscription subscription) {
        JsonNode root;
        try {
            root = this.mapper.readTree(response.getEntityInputStream());
        } catch (IOException e) {
            logger.error(BILLING_MARKER,
                    String.format("Subscription could not be parsed: %s", e.getMessage()));
            return;
        }
        JsonNode subJson = root.get("subscription");
        int id = subJson.get("id").asInt();
        logger.info(String.format("Subscription (external: %d) was created.", id));
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
