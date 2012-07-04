package com.dynamo.cr.web2.client.activity;

import com.dynamo.cr.web2.client.ClientFactory;
import com.dynamo.cr.web2.client.Defold;
import com.dynamo.cr.web2.client.ProductInfo;
import com.dynamo.cr.web2.client.ProductInfoList;
import com.dynamo.cr.web2.client.ResourceCallback;
import com.dynamo.cr.web2.client.UserSubscriptionInfo;
import com.dynamo.cr.web2.client.place.DashboardPlace;
import com.dynamo.cr.web2.client.place.SubscriptionPlace;
import com.dynamo.cr.web2.client.ui.SubscriptionView;
import com.google.gwt.activity.shared.AbstractActivity;
import com.google.gwt.event.shared.EventBus;
import com.google.gwt.http.client.Request;
import com.google.gwt.http.client.Response;
import com.google.gwt.user.client.Window;
import com.google.gwt.user.client.ui.AcceptsOneWidget;

public class SubscriptionActivity extends AbstractActivity implements SubscriptionView.Presenter {
    private ClientFactory clientFactory;

    public SubscriptionActivity(SubscriptionPlace place, ClientFactory clientFactory) {
        this.clientFactory = clientFactory;
    }

    public void loadProducts() {
        final SubscriptionView subscriptionView = clientFactory.getSubscriptionView();
        final Defold defold = clientFactory.getDefold();
        defold.getResource("/products", new ResourceCallback<ProductInfoList>() {

            @Override
            public void onSuccess(ProductInfoList productInfoList, Request request,
                    Response response) {
                subscriptionView.setProductInfoList(productInfoList);
            }

            @Override
            public void onFailure(Request request, Response response) {
                subscriptionView.clear();
            }
        });
    }

    private void loadSubscription() {
        final SubscriptionView subscriptionView = clientFactory.getSubscriptionView();
        final Defold defold = clientFactory.getDefold();
        defold.getResource("/users/" + defold.getUserId() + "/subscription",
                new ResourceCallback<UserSubscriptionInfo>() {

                    @Override public void onSuccess(UserSubscriptionInfo subscription,
                            Request request, Response response) {
                        subscriptionView.setUserSubscription(subscription);
                    }

                    @Override public void onFailure(Request request, Response response) {
                        defold.showErrorMessage("Subscription data could not be loaded.");
                    }
                });
    }

    @Override
    public void start(AcceptsOneWidget containerWidget, EventBus eventBus) {
        final SubscriptionView subscriptionView = clientFactory.getSubscriptionView();
        containerWidget.setWidget(subscriptionView.asWidget());
        subscriptionView.setPresenter(this);
        subscriptionView.clear();
        loadProducts();
        loadSubscription();
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
            final Defold defold = clientFactory.getDefold();
            defold.putResource("/users/" + defold.getUserId() + "/subscription?product=" + product.getId(), "",
                    new ResourceCallback<String>() {

                        @Override
                        public void onSuccess(String data,
                                Request request, Response response) {
                            clientFactory.getPlaceController().goTo(new DashboardPlace());
                        }

                        @Override
                        public void onFailure(Request request, Response response) {
                            defold.showErrorMessage("Subscription could not be migrated to the product " + product.getName() + ".");
                        }
                    });
        }
    }

    public void onReactivate(UserSubscriptionInfo subscription) {
        final Defold defold = clientFactory.getDefold();
        defold.putResource("/users/" + defold.getUserId() + "/subscription?state=ACTIVE", "",
                new ResourceCallback<String>() {

                    @Override
                    public void onSuccess(String data,
                            Request request, Response response) {
                        clientFactory.getPlaceController().goTo(new DashboardPlace());
                    }

                    @Override
                    public void onFailure(Request request, Response response) {
                        defold.showErrorMessage("Subscription could not be reactivated.");
                    }
                });
    }

    public void onTerminate(UserSubscriptionInfo subscription) {
        final Defold defold = clientFactory.getDefold();
        defold.deleteResource("/users/" + defold.getUserId() + "/subscription",
                new ResourceCallback<String>() {

                    @Override
                    public void onSuccess(String data,
                            Request request, Response response) {
                        clientFactory.getPlaceController().goTo(new DashboardPlace());
                    }

                    @Override
                    public void onFailure(Request request, Response response) {
                        defold.showErrorMessage("Subscription could not be terminated.");
                    }
                });
    }
}