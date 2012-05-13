package com.dynamo.cr.web2.client.activity;

import com.dynamo.cr.web2.client.ClientFactory;
import com.dynamo.cr.web2.client.Defold;
import com.dynamo.cr.web2.client.place.ProductInfoPlace;
import com.dynamo.cr.web2.client.ui.ProductInfoView;
import com.google.gwt.activity.shared.AbstractActivity;
import com.google.gwt.event.shared.EventBus;
import com.google.gwt.http.client.Request;
import com.google.gwt.http.client.RequestBuilder;
import com.google.gwt.http.client.RequestCallback;
import com.google.gwt.http.client.RequestException;
import com.google.gwt.http.client.Response;
import com.google.gwt.user.client.ui.AcceptsOneWidget;

public class ProductInfoActivity extends AbstractActivity implements ProductInfoView.Presenter {
    private ClientFactory clientFactory;

    public ProductInfoActivity(ProductInfoPlace place, ClientFactory clientFactory) {
        this.clientFactory = clientFactory;
    }

    @Override
    public void start(AcceptsOneWidget containerWidget, EventBus eventBus) {
        ProductInfoView productInfoView = clientFactory.getProductInfoView();
        containerWidget.setWidget(productInfoView.asWidget());
        productInfoView.setPresenter(this);
        productInfoView.reset();
    }

    @Override
    public void registerProspect(String email) {
        final ProductInfoView productInfoView = clientFactory.getProductInfoView();
        final Defold defold = clientFactory.getDefold();
        String url = defold.getUrl() + "/prospects/" + email;
        RequestBuilder requestBuilder = new RequestBuilder(RequestBuilder.PUT, url);
        requestBuilder.setHeader("Accept", "application/json");
        requestBuilder.setHeader("Content-Type", "application/json");
        try {
            requestBuilder.sendRequest("", new RequestCallback() {

                @Override
                public void onResponseReceived(Request request, Response response) {
                    int statusCode = response.getStatusCode();
                    switch (statusCode) {
                        case Response.SC_OK:
                            productInfoView.showRegisterConfirmation();
                            break;
                        case Response.SC_CONFLICT:
                            defold.showErrorMessage("Your email has already been registered.");
                            break;
                        default:
                            defold.showErrorMessage("An error occurred: " + response.getStatusText());
                            break;
                    }
                }

                @Override
                public void onError(Request request, Throwable exception) {
                    // NOTE: We don't pass this one through. Errors related to timeout etc
                    defold.showErrorMessage("Network error");
                }
            });
        } catch (RequestException e) {
            // NOTE: We don't pass this one through. Errors related to timeout etc
            defold.showErrorMessage("Network error");
        }
    }
}