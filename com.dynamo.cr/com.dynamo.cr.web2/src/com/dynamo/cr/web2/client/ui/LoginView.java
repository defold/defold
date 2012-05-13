package com.dynamo.cr.web2.client.ui;

import com.google.gwt.core.client.GWT;
import com.google.gwt.event.dom.client.ClickEvent;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.uibinder.client.UiHandler;
import com.google.gwt.user.client.ui.Button;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.Widget;

public class LoginView extends Composite {

    public interface Presenter {
        void loginGoogle();
        void loginYahoo();
    }

    private static final Binder binder = GWT.create(Binder.class);
    @UiField Button loginGoogleButton;
    @UiField Button loginYahooButton;
    private LoginView.Presenter listener;

    interface Binder extends UiBinder<Widget, LoginView> {
    }

    public LoginView() {
        initWidget(binder.createAndBindUi(this));
    }

    public void setPresenter(LoginView.Presenter listener) {
        this.listener = listener;
    }

    @UiHandler("loginGoogleButton")
    void onLoginGoogleButtonClick(ClickEvent event) {
        listener.loginGoogle();
    }

    @UiHandler("loginYahooButton")
    void onLoginYahooButtonClick(ClickEvent event) {
        listener.loginYahoo();
    }

}
