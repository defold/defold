package com.dynamo.cr.web2.client.ui;

import com.google.gwt.core.client.GWT;
import com.google.gwt.event.dom.client.ClickEvent;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.uibinder.client.UiHandler;
import com.google.gwt.user.client.Cookies;
import com.google.gwt.user.client.ui.Button;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.Label;
import com.google.gwt.user.client.ui.PasswordTextBox;
import com.google.gwt.user.client.ui.TextBox;
import com.google.gwt.user.client.ui.Widget;

public class LoginView extends Composite {

    public interface Presenter {
        void login(String email, String password);
    }

    private static final Binder binder = GWT.create(Binder.class);
    @UiField
    TextBox emailTextBox;
    @UiField
    PasswordTextBox passwordTextBox;
    @UiField
    Button loginButton;
    @UiField
    Label errorLabel;
    private LoginView.Presenter listener;

    interface Binder extends UiBinder<Widget, LoginView> {
    }

    public LoginView() {
        initWidget(binder.createAndBindUi(this));
        String email = Cookies.getCookie("email");
        if (email != null)
            emailTextBox.setText(email);
    }

    @UiHandler("loginButton")
    void onLoginButtonClick(ClickEvent event) {
        listener.login(emailTextBox.getText(), passwordTextBox.getText());
    }

    public void setPresenter(LoginView.Presenter listener) {
        this.listener = listener;
    }

    public void setError(String error) {
        errorLabel.setVisible(error != null);
        passwordTextBox.setText(error);
    }
}
