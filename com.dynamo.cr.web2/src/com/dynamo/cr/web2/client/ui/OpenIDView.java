package com.dynamo.cr.web2.client.ui;

import com.google.gwt.core.client.GWT;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.user.client.ui.Button;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.DeckPanel;
import com.google.gwt.user.client.ui.InlineLabel;
import com.google.gwt.user.client.ui.Label;
import com.google.gwt.user.client.ui.TextArea;
import com.google.gwt.user.client.ui.Widget;
import com.google.gwt.uibinder.client.UiHandler;
import com.google.gwt.event.dom.client.ClickEvent;
import com.sun.xml.internal.bind.v2.runtime.reflect.Lister;

public class OpenIDView extends Composite {

    public interface Presenter {
        void register();
    }

    private static final Binder binder = GWT.create(Binder.class);
    @UiField DeckPanel deckPanel;
    @UiField Label errorLabel;
    @UiField InlineLabel emailLabel;
    @UiField InlineLabel firstNameLabel;
    @UiField InlineLabel lastNameLabel;
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
        errorLabel.setVisible(false);
        firstNameLabel.setText("");
        lastNameLabel.setText("");
        emailLabel.setText("");
    }

    public void setError(String message) {
        errorLabel.setText(message);
        errorLabel.setVisible(true);
    }

    public void setConfirmRegistrationData(String firstName, String lastName, String email) {
        firstNameLabel.setText(firstName);
        lastNameLabel.setText(lastName);
        emailLabel.setText(email);
    }

    @UiHandler("registerButton")
    void onRegisterButtonClick(ClickEvent event) {
        listener.register();
    }
}
