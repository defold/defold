package com.dynamo.cr.server.billing;

import java.net.URI;

import javax.ws.rs.core.MediaType;
import javax.ws.rs.core.UriBuilder;

import org.codehaus.jackson.map.ObjectMapper;
import org.codehaus.jackson.node.ObjectNode;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.proto.Config.Configuration;
import com.dynamo.cr.server.model.Product;
import com.dynamo.cr.server.model.UserSubscription;
import com.google.inject.Inject;
import com.sun.jersey.api.client.Client;
import com.sun.jersey.api.client.ClientResponse;
import com.sun.jersey.api.client.WebResource;
import com.sun.jersey.api.client.config.ClientConfig;
import com.sun.jersey.api.client.config.DefaultClientConfig;
import com.sun.jersey.api.client.filter.HTTPBasicAuthFilter;

public class ChargifyService implements IBillingProvider {

    private static Logger logger = LoggerFactory.getLogger(ChargifyService.class);

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

    private boolean verifyAndLogResponse(ClientResponse response, UserSubscription subscription,
            String operation) {
        int status = response.getStatus();
        if (status >= 200 && status < 300) {
            logger.info(String.format("Subscription %d (external: %d) was %s.", subscription.getId(),
                    subscription.getExternalId(),
                    operation));
            return true;
        } else {
            logger.error(String.format("Subscription %d (external: %d) failed to be %s: %d.", subscription.getId(),
                    subscription.getExternalId(), operation, status));
            return false;
        }
    }

    @Override
    public boolean reactivateSubscription(UserSubscription subscription) {
        ClientResponse response = chargifyResource
                .path(String.format("/%d/reactivate.json", subscription.getExternalId()))
                .type(MediaType.APPLICATION_JSON_TYPE).accept(MediaType.APPLICATION_JSON_TYPE)
                .put(ClientResponse.class, "");
        return verifyAndLogResponse(response, subscription, "reactivated");
    }

    @Override
    public boolean migrateSubscription(UserSubscription subscription, Product newProduct) {
        ObjectNode root = mapper.createObjectNode();
        ObjectNode migration = mapper.createObjectNode();
        migration.put("product_handle", newProduct.getHandle());
        root.put("migration", migration);
        ClientResponse response = chargifyResource
                .path(String.format("/%d/migrations.json", subscription.getExternalId()))
                .type(MediaType.APPLICATION_JSON_TYPE).accept(MediaType.APPLICATION_JSON_TYPE)
                .post(ClientResponse.class, root.toString());
        return verifyAndLogResponse(response, subscription, "migrated");
    }

    @Override
    public boolean cancelSubscription(UserSubscription subscription) {
        ClientResponse response = chargifyResource
                .path(String.format("/%d.json", subscription.getExternalId()))
                .type(MediaType.APPLICATION_JSON_TYPE).accept(MediaType.APPLICATION_JSON_TYPE)
                .delete(ClientResponse.class);
        return verifyAndLogResponse(response, subscription, "canceled");
    }

}
