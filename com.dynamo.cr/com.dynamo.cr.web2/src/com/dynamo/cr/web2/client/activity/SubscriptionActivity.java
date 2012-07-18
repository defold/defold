package com.dynamo.cr.web2.client.activity;

import com.dynamo.cr.web2.client.ClientFactory;
import com.dynamo.cr.web2.client.Defold;
import com.dynamo.cr.web2.client.ProductInfo;
import com.dynamo.cr.web2.client.ProductInfoList;
import com.dynamo.cr.web2.client.ResourceCallback;
import com.dynamo.cr.web2.client.UserSubscriptionInfo;
import com.dynamo.cr.web2.client.place.SubscriptionPlace;
import com.dynamo.cr.web2.client.ui.SubscriptionView;
import com.google.gwt.activity.shared.AbstractActivity;
import com.google.gwt.event.shared.EventBus;
import com.google.gwt.http.client.Request;
import com.google.gwt.http.client.Response;
import com.google.gwt.user.client.Window;
import com.google.gwt.user.client.ui.AcceptsOneWidget;

public class SubscriptionActivity extends AbstractActivity implements SubscriptionView.Presenter {
    private SubscriptionPlace place;
    private ClientFactory clientFactory;

    public SubscriptionActivity(SubscriptionPlace place, ClientFactory clientFactory) {
        this.place = place;
        this.clientFactory = clientFactory;
    }

    @Override
    public void start(AcceptsOneWidget containerWidget, EventBus eventBus) {
        // Check if we received a subscription id, bail out and convert it to
        // place-token
        String subscriptionId = Window.Location.getParameter("subscription_id");
        if (subscriptionId != null) {
            Window.Location.replace(Window.Location.createUrlBuilder().removeParameter("subscription_id").buildString()
                    + subscriptionId);
            return;
        }
        final SubscriptionView subscriptionView = clientFactory.getSubscriptionView();
        containerWidget.setWidget(subscriptionView.asWidget());
        subscriptionView.setPresenter(this);
        subscriptionView.clear();
        loadGravatar();
        loadProducts();
        loadSubscription();
    }

    private void showErrorMessage(String message) {
        Defold defold = clientFactory.getDefold();
        defold.showErrorMessage(message);
        SubscriptionView subscriptionView = clientFactory.getSubscriptionView();
        subscriptionView.setLoading(false);
    }

    private void loadGravatar() {
        final SubscriptionView subscriptionView = clientFactory.getSubscriptionView();
        Defold defold = clientFactory.getDefold();
        String email = defold.getEmail();

        String firstName = defold.getFirstName();
        String lastName = defold.getLastName();
        subscriptionView.setUserInfo(firstName, lastName, email);
    }

    private void loadProducts() {
        final SubscriptionView subscriptionView = clientFactory.getSubscriptionView();
        subscriptionView.setLoading(true);
        final Defold defold = clientFactory.getDefold();
        defold.getResource("/products", new ResourceCallback<ProductInfoList>() {

            @Override
            public void onSuccess(ProductInfoList productInfoList, Request request,
                    Response response) {
                if (productInfoList != null) {
                    subscriptionView.setProductInfoList(productInfoList);
                } else {
                    showErrorMessage(response.getText());
                }
            }

            @Override
            public void onFailure(Request request, Response response) {
                showErrorMessage("Could not load products (network error).");
            }
        });
    }

    private void loadSubscription() {
        final SubscriptionView subscriptionView = clientFactory.getSubscriptionView();
        subscriptionView.setLoading(true);
        final Defold defold = clientFactory.getDefold();
        defold.getResource("/users/" + defold.getUserId() + "/subscription",
                new ResourceCallback<UserSubscriptionInfo>() {

                    @Override public void onSuccess(UserSubscriptionInfo subscription,
                            Request request, Response response) {
                        if (subscription != null) {
                            subscriptionView.setUserSubscription(subscription);
                            String newSubscriptionId = place.getSubscriptionId();
                            if (newSubscriptionId != null) {
                                if (subscription.getCreditCardInfo().getMaskedNumber().isEmpty()) {
                                    createSubscription(newSubscriptionId);
                                } else {
                                    updateSubscription(newSubscriptionId);
                                }
                            }
                        } else {
                            showErrorMessage(response.getText());
                        }
                    }

                    @Override public void onFailure(Request request, Response response) {
                        showErrorMessage("Subscription data could not be loaded (network error).");
                    }
                });
    }

    private void createSubscription(String newSubscriptionId) {
        final SubscriptionView subscriptionView = clientFactory.getSubscriptionView();
        subscriptionView.setLoading(true);
        final Defold defold = clientFactory.getDefold();
        defold.postResourceRetrieve("/users/" + defold.getUserId() + "/subscription?external_id=" + newSubscriptionId,
                "",
                new ResourceCallback<UserSubscriptionInfo>() {

                    @Override
                    public void onSuccess(UserSubscriptionInfo subscription,
                            Request request, Response response) {
                        if (subscription != null) {
                            subscriptionView.setUserSubscription(subscription);
                        } else {
                            showErrorMessage(response.getText());
                        }
                    }

                    @Override
                    public void onFailure(Request request, Response response) {
                        showErrorMessage("Subscription data could not be created (network error).");
                    }
                });
    }

    private void updateSubscription(String newSubscriptionId) {
        final SubscriptionView subscriptionView = clientFactory.getSubscriptionView();
        subscriptionView.setLoading(true);
        final Defold defold = clientFactory.getDefold();
        defold.putResourceRetrieve("/users/" + defold.getUserId() + "/subscription?external_id=" + newSubscriptionId,
                "",
                new ResourceCallback<UserSubscriptionInfo>() {

                    @Override
                    public void onSuccess(UserSubscriptionInfo subscription,
                            Request request, Response response) {
                        if (subscription != null) {
                            subscriptionView.setUserSubscription(subscription);
                        } else {
                            showErrorMessage(response.getText());
                        }
                    }

                    @Override
                    public void onFailure(Request request, Response response) {
                        showErrorMessage("Subscription data could not be updated (network error).");
                    }
                });
    }

    @Override
    public void onMigrate(UserSubscriptionInfo subscription, final ProductInfo product) {
        String updateUrl = subscription.getUpdateURL();
        if (updateUrl == null || updateUrl.isEmpty()) {
            Defold defold = this.clientFactory.getDefold();
            StringBuilder builder = new StringBuilder(product.getSignupURL());
            builder.append("?first_name=").append(defold.getFirstName()).append("&last_name=")
                    .append(defold.getLastName()).append("&email=").append(defold.getEmail());
            Window.open(builder.toString(), "_self", "");
        } else {
            final SubscriptionView subscriptionView = clientFactory.getSubscriptionView();
            subscriptionView.setLoading(true);
            final Defold defold = clientFactory.getDefold();
            defold.putResourceRetrieve("/users/" + defold.getUserId() + "/subscription?product=" + product.getId(), "",
                    new ResourceCallback<UserSubscriptionInfo>() {

                        @Override
                        public void onSuccess(UserSubscriptionInfo subscription,
                                Request request, Response response) {
                            if (subscription != null) {
                                final SubscriptionView subscriptionView = clientFactory.getSubscriptionView();
                                subscriptionView.setUserSubscription(subscription);
                            } else {
                                showErrorMessage(response.getText());
                            }
                        }

                        @Override
                        public void onFailure(Request request, Response response) {
                            showErrorMessage("Subscription could not be updated (network error).");
                        }
                    });
        }
    }

    public void onReactivate(UserSubscriptionInfo subscription) {
        final SubscriptionView subscriptionView = clientFactory.getSubscriptionView();
        subscriptionView.setLoading(true);
        final Defold defold = clientFactory.getDefold();
        defold.putResourceRetrieve("/users/" + defold.getUserId() + "/subscription?state=ACTIVE", "",
                new ResourceCallback<UserSubscriptionInfo>() {

                    @Override
                    public void onSuccess(UserSubscriptionInfo subscription,
                            Request request, Response response) {
                        if (subscription != null) {
                            final SubscriptionView subscriptionView = clientFactory.getSubscriptionView();
                            subscriptionView.setUserSubscription(subscription);
                        } else {
                            showErrorMessage(response.getText());
                        }
                    }

                    @Override
                    public void onFailure(Request request, Response response) {
                        showErrorMessage("Subscription could not be reactivated (network error).");
                    }
                });
    }

    public void onTerminate(UserSubscriptionInfo subscription) {
        final SubscriptionView subscriptionView = clientFactory.getSubscriptionView();
        subscriptionView.setLoading(true);
        final Defold defold = clientFactory.getDefold();
        defold.deleteResource("/users/" + defold.getUserId() + "/subscription",
                new ResourceCallback<String>() {

                    @Override
                    public void onSuccess(String data,
                            Request request, Response response) {
                        if (response.getStatusCode() >= 200 && response.getStatusCode() < 300) {
                            loadSubscription();
                        } else {
                            showErrorMessage(response.getText());
                        }
                    }

                    @Override
                    public void onFailure(Request request, Response response) {
                        showErrorMessage("Subscription could not be terminated (network error).");
                    }
                });
    }

    @Override
    public void onEditCreditCard(UserSubscriptionInfo subscription) {
        Window.open(subscription.getUpdateURL(), "_self", "");
    }
}