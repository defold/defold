package com.dynamo.cr.web2.client.activity;

import com.dynamo.cr.web2.client.ClientFactory;
import com.dynamo.cr.web2.client.Defold;
import com.dynamo.cr.web2.client.ResourceCallback;
import com.dynamo.cr.web2.client.place.SettingsPlace;
import com.dynamo.cr.web2.client.ui.SettingsView;
import com.google.gwt.activity.shared.AbstractActivity;
import com.google.gwt.event.shared.EventBus;
import com.google.gwt.http.client.Request;
import com.google.gwt.http.client.Response;
import com.google.gwt.user.client.ui.AcceptsOneWidget;

public class SettingsActivity extends AbstractActivity implements SettingsView.Presenter {
    private ClientFactory clientFactory;

    public SettingsActivity(SettingsPlace place, ClientFactory clientFactory) {
        this.clientFactory = clientFactory;
    }

    private void loadSubscriptionStatus() {
        final SettingsView settingsView = clientFactory.getSettingsView();
        final Defold defold = clientFactory.getDefold();
        defold.getStringResource("/news_list/" + defold.getUserId() + "/subscribe",
                new ResourceCallback<String>() {

                    @Override
                    public void onSuccess(String status,
                            Request request, Response response) {
                        if (response.getStatusCode() < 400 && status != null) {
                            settingsView.setSubscriptionStatus(status.trim().equals("true"));
                            settingsView.setAuthToken(Defold.getCookie("auth"));
                            settingsView.setVisible(true);
                        } else {
                            defold.showErrorMessage("Subscription status could not be loaded.");
                        }
                    }

                    @Override
                    public void onFailure(Request request, Response response) {
                        defold.showErrorMessage("Subscription status could not be loaded.");
                    }
                });
    }

    @Override
    public void start(AcceptsOneWidget containerWidget, EventBus eventBus) {
        final SettingsView settingsView = clientFactory.getSettingsView();
        containerWidget.setWidget(settingsView.asWidget());
        settingsView.setVisible(false);
        settingsView.setPresenter(this);
        loadGravatar();
        loadSubscriptionStatus();
    }

    private void loadGravatar() {
        final SettingsView settingsView = clientFactory.getSettingsView();
        Defold defold = clientFactory.getDefold();
        String email = defold.getEmail();

        String firstName = defold.getFirstName();
        String lastName = defold.getLastName();
        settingsView.setUserInfo(firstName, lastName, email);
    }

    @Override
    public void setSubscriptionStatus(boolean status) {
        final Defold defold = clientFactory.getDefold();

        ResourceCallback<String> callback = new ResourceCallback<String>() {

            @Override
            public void onSuccess(String result, Request request,
                    Response response) {
                if (response.getStatusCode() > 300) {
                    defold.showErrorMessage("Subscription status could not be changed.");
                }
            }

            @Override
            public void onFailure(Request request, Response response) {
                defold.showErrorMessage("Subscription status could not be changed.");
            }
        };

        if (status) {
            defold.putResource("/news_list/" + defold.getUserId() + "/subscribe", "", callback);
        } else {
            defold.deleteResource("/news_list/" + defold.getUserId() + "/subscribe", callback);
        }
    }

}
