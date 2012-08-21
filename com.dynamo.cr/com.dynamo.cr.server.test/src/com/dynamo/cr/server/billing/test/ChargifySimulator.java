package com.dynamo.cr.server.billing.test;

import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.ws.rs.WebApplicationException;
import javax.ws.rs.core.UriBuilder;

import com.dynamo.cr.proto.Config.BillingProduct;
import com.dynamo.cr.proto.Config.Configuration;
import com.dynamo.cr.server.billing.ChargifyService;
import com.dynamo.cr.server.billing.IBillingProvider;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.model.UserSubscription;
import com.dynamo.cr.server.util.ChargifyUtil;
import com.google.inject.Inject;
import com.sun.jersey.api.client.Client;
import com.sun.jersey.api.client.ClientResponse;
import com.sun.jersey.api.client.WebResource;
import com.sun.jersey.api.client.config.ClientConfig;
import com.sun.jersey.api.client.config.DefaultClientConfig;
import com.sun.jersey.api.representation.Form;

/**
 * This class replicates the chargify service by calling it and then manually
 * sending webhooks. Webhooks can't be received from the actual chargify service
 * to the test environment.
 *
 * @author rasv
 *
 */
public class ChargifySimulator implements IBillingProvider {

    class Webhook {
        public Webhook(String event, Form form) {
            this.event = event;
            this.form = form;
        }

        public String event;
        public Form form;
    }

    int port = 6500;

    private WebResource webhookResource;
    private List<Webhook> webhooks;
    private String sharedKey;
    private IBillingProvider billingProvider;
    private Map<Integer, BillingProduct> products;

    private boolean delayedSignUpFailure;

    @Inject
    public ChargifySimulator(Configuration configuration) {
        this.webhooks = new ArrayList<Webhook>();
        this.billingProvider = new ChargifyService(configuration);

        ClientConfig clientConfig = new DefaultClientConfig();
        Client client = Client.create(clientConfig);
        URI uri = UriBuilder.fromUri(String.format("http://localhost/chargify")).port(port).build();
        this.webhookResource = client.resource(uri);

        this.sharedKey = configuration.getBillingSharedKey();
        this.delayedSignUpFailure = false;

        this.products = new HashMap<Integer, BillingProduct>();
        for (BillingProduct product : configuration.getProductsList()) {
            this.products.put(product.getId(), product);
        }
    }

    public void setDelayedSignUpFailure(boolean delayedSignUpFailure) {
        this.delayedSignUpFailure = delayedSignUpFailure;
    }

    public void sendWebhooks() throws UnsupportedEncodingException {
        for (Webhook hook : this.webhooks) {
            ClientResponse response = ChargifyUtil.fakeWebhook(this.webhookResource, hook.event, hook.form, true,
                    this.sharedKey);
            if ((response.getStatus() / 100) != 2) {
                throw new WebApplicationException(response.getStatus());
            }
        }
        this.webhooks.clear();
    }

    public void addWebhook(Webhook hook) {
        this.webhooks.add(hook);
    }

    public void clearWebhooks() {
        this.webhooks.clear();
    }

    /**
     * Simulate hosted sign up
     *
     * @return subscription json at success, null at failure
     */
    @Override
    public UserSubscription signUpUser(BillingProduct product, User user, String creditCard, String ccExpirationMonth,
            String ccExpirationYear, String cvv) {
        UserSubscription subscription = billingProvider.signUpUser(product, user, creditCard, ccExpirationMonth,
                ccExpirationYear, cvv);
        Form f = new Form();
        f.add("payload[subscription][id]", subscription.getExternalId());
        if (this.delayedSignUpFailure) {
            addWebhook(new Webhook(ChargifyUtil.SIGNUP_FAILURE_WH, f));
        } else {
            addWebhook(new Webhook(ChargifyUtil.SIGNUP_SUCCESS_WH, f));
        }
        return subscription;
    }

    @Override
    public void cancelSubscription(UserSubscription subscription) {
        this.billingProvider.cancelSubscription(subscription);
        Form f = new Form();
        f.add("payload[subscription][id]", subscription.getExternalId().toString());
        f.add("payload[subscription][state]", "canceled");
        addWebhook(new Webhook(ChargifyUtil.SUBSCRIPTION_STATE_CHANGE_WH, f));
    }

    @Override
    public void reactivateSubscription(UserSubscription subscription) throws WebApplicationException {
        this.billingProvider.reactivateSubscription(subscription);
        Form f = new Form();
        f.add("payload[subscription][id]", subscription.getExternalId().toString());
        f.add("payload[subscription][state]", "active");
        addWebhook(new Webhook(ChargifyUtil.SUBSCRIPTION_STATE_CHANGE_WH, f));
    }

    @Override
    public void migrateSubscription(UserSubscription subscription, BillingProduct product) throws WebApplicationException {
        BillingProduct previousProduct = this.products.get((int) subscription.getProductId().longValue());
        this.billingProvider.migrateSubscription(subscription, product);
        Form f = new Form();
        f.add("payload[subscription][id]", subscription.getExternalId().toString());
        f.add("payload[subscription][state]", "active");
        f.add("payload[subscription][product][handle]", product.getHandle());
        f.add("payload[previous_product][handle]", previousProduct.getHandle());
        addWebhook(new Webhook(ChargifyUtil.SUBSCRIPTION_PRODUCT_CHANGE_WH, f));
    }

    @Override
    public UserSubscription getSubscription(Long subscriptionId) throws WebApplicationException {
        return this.billingProvider.getSubscription(subscriptionId);
    }
}
