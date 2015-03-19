package com.dynamo.cr.web2.client.ui;

import com.google.gwt.core.client.GWT;
import com.google.gwt.event.dom.client.ClickEvent;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.uibinder.client.UiHandler;
import com.google.gwt.user.client.ui.CheckBox;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.HTMLPanel;
import com.google.gwt.user.client.ui.InlineLabel;
import com.google.gwt.user.client.ui.Widget;

public class SettingsView extends Composite {

    public interface Presenter {
        void setSubscriptionStatus(boolean status);
    }

    private Presenter listener;
    private static DashboardUiBinder uiBinder = GWT.create(DashboardUiBinder.class);
    @UiField SideBar sideBar;
    @UiField HTMLPanel settingsPanel;
    @UiField CheckBox subscriptionStatus;
    @UiField InlineLabel authToken;

    interface DashboardUiBinder extends UiBinder<Widget, SettingsView> {
    }

    public SettingsView() {
        initWidget(uiBinder.createAndBindUi(this));
    }

    public void setPresenter(SettingsView.Presenter listener) {
        sideBar.setActivePage("settings");
        this.listener = listener;
    }

    public void setVisible(boolean visible) {
        settingsPanel.setVisible(visible);
    }

    public void setUserInfo(String firstName, String lastName, String email) {
        sideBar.setUserInfo(firstName, lastName, email);
    }

    public void setSubscriptionStatus(boolean status) {
        this.subscriptionStatus.setValue(status);
    }

    public void setAuthToken(String authToken) {
        if (authToken == null) {
            this.authToken.setText("invalid");
        } else {
            this.authToken.setText(authToken);
        }
    }

    @UiHandler("subscriptionStatus")
    void onSubscribe(ClickEvent event) {
        this.listener.setSubscriptionStatus(subscriptionStatus.getValue());
    }

}
