package com.dynamo.cr.server.billing.test;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.util.ArrayList;
import java.util.List;

import javax.ws.rs.core.MediaType;

import org.apache.commons.io.IOUtils;
import org.codehaus.jackson.JsonNode;
import org.codehaus.jackson.map.ObjectMapper;
import org.codehaus.jackson.node.ObjectNode;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.server.model.Product;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.util.ChargifyUtil;
import com.sun.jersey.api.client.ClientResponse;
import com.sun.jersey.api.client.WebResource;
import com.sun.jersey.api.representation.Form;

public class ChargifySimulator {

    private static Logger logger = LoggerFactory.getLogger(ChargifySimulator.class);

    class Webhook {
        public Webhook(String event, Form form) {
            this.event = event;
            this.form = form;
        }

        public String event;
        public Form form;
    }

    int port = 6500;

    private WebResource chargifyResource;
    private WebResource webhookResource;
    private List<Webhook> webhooks;
    private ObjectMapper mapper;
    private String sharedKey;

    public ChargifySimulator(WebResource chargifyResource, WebResource webhookResource, String sharedKey) {
        this.webhooks = new ArrayList<Webhook>();
        this.chargifyResource = chargifyResource;
        this.webhookResource = webhookResource;
        this.sharedKey = sharedKey;
        this.mapper = new ObjectMapper();
    }

    public boolean sendWebhooks() throws UnsupportedEncodingException {
        for (Webhook hook : this.webhooks) {
            ClientResponse response = ChargifyUtil.fakeWebhook(this.webhookResource, hook.event, hook.form, true,
                    this.sharedKey);
            if (response.getStatus() < 200 || response.getStatus() >= 300) {
                return false;
            }
        }
        this.webhooks.clear();
        return true;
    }

    private boolean validateResponse(ClientResponse response) throws IOException {
        if (response.getStatus() >= 200 && response.getStatus() < 300) {
            return true;
        } else {
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            IOUtils.copy(response.getEntityInputStream(), out);
            logger.error(new String(out.toByteArray()));
            return false;
        }
    }

    /**
     * Simulate hosted sign up
     *
     * @return subscription json at success, null at failure
     */
    public JsonNode signUpUser(Product product, User user, String creditCard, boolean failLater) throws Exception {
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
        if (!validateResponse(response)) {
            return null;
        }
        JsonNode c = mapper.readTree(response.getEntityInputStream());
        int customerId = c.get("customer").get("id").getIntValue();

        root = this.mapper.createObjectNode();
        ObjectNode subscription = this.mapper.createObjectNode();
        subscription.put("product_handle", product.getHandle());
        subscription.put("customer_id", customerId);
        ObjectNode cc = this.mapper.createObjectNode();
        cc.put("full_number", creditCard);
        cc.put("expiration_month", "12");
        cc.put("expiration_year", "2020");
        subscription.put("credit_card_attributes", cc);
        root.put("subscription", subscription);
        response = chargifyResource.path("/subscriptions.json").type(MediaType.APPLICATION_JSON_TYPE)
                .accept(MediaType.APPLICATION_JSON_TYPE).post(ClientResponse.class, root.toString());
        if (validateResponse(response)) {
            JsonNode s = mapper.readTree(response.getEntityInputStream());
            int subscriptionId = s.get("subscription").get("id").getIntValue();
            Form f = new Form();
            f.add("payload[subscription][id]", subscriptionId);
            if (failLater) {
                this.webhooks.add(new Webhook("signup_failure", f));
            } else {
                this.webhooks.add(new Webhook("signup_success", f));
            }
            return s;
        } else {
            return null;
        }
    }
}
