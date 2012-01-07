package com.dynamo.cr.web2.client.ui;

import com.google.gwt.core.client.GWT;
import com.google.gwt.event.dom.client.ClickEvent;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.uibinder.client.UiHandler;
import com.google.gwt.user.client.ui.Button;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.DeckPanel;
import com.google.gwt.user.client.ui.InlineLabel;
import com.google.gwt.user.client.ui.TextArea;
import com.google.gwt.user.client.ui.TextBox;
import com.google.gwt.user.client.ui.Widget;

public class OpenIDView extends Composite {

    public interface Presenter {
        void register(String registrationKey);
    }

    private static final Binder binder = GWT.create(Binder.class);
    @UiField DeckPanel deckPanel;
    @UiField InlineLabel emailLabel;
    @UiField InlineLabel firstNameLabel;
    @UiField InlineLabel lastNameLabel;
    @UiField TextBox registrationKey;
    @UiField TextArea eula;
    @UiField Button registerButton;

    private OpenIDView.Presenter listener;

    interface Binder extends UiBinder<Widget, OpenIDView> {
    }

    public OpenIDView() {
        initWidget(binder.createAndBindUi(this));
    }

    public void showConfirmRegistrationPanel() {
        deckPanel.setVisible(true);
        deckPanel.showWidget(0);
    }

    public void showLoginSuccessfulPanel() {
        deckPanel.setVisible(true);
        deckPanel.showWidget(1);
    }

    public void hideDeckPanel() {
       deckPanel.setVisible(false);
    }

    public void setPresenter(OpenIDView.Presenter listener) {
        this.listener = listener;
        hideDeckPanel();
        firstNameLabel.setText("");
        lastNameLabel.setText("");
        emailLabel.setText("");
    }

    public void setConfirmRegistrationData(String firstName, String lastName, String email) {
        firstNameLabel.setText(firstName);
        lastNameLabel.setText(lastName);
        emailLabel.setText(email);
    }

    @UiHandler("registerButton")
    void onRegisterButtonClick(ClickEvent event) {
        listener.register(registrationKey.getText());
    }
}
