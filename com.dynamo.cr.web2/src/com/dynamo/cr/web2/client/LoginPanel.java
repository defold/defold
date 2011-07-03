package com.dynamo.cr.web2.client;

import java.util.Date;

import com.google.gwt.core.client.GWT;
import com.google.gwt.event.dom.client.ClickEvent;
import com.google.gwt.http.client.Request;
import com.google.gwt.http.client.RequestBuilder;
import com.google.gwt.http.client.RequestCallback;
import com.google.gwt.http.client.RequestException;
import com.google.gwt.http.client.Response;
import com.google.gwt.json.client.JSONObject;
import com.google.gwt.json.client.JSONParser;
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

public class LoginPanel extends Composite {

    private Defold defold;
    private static final Binder binder = GWT.create(Binder.class);
    @UiField
    TextBox emailTextBox;
    @UiField
    PasswordTextBox passwordTextBox;
    @UiField
    Button loginButton;
    @UiField
    Label errorLabel;

    interface Binder extends UiBinder<Widget, LoginPanel> {
    }

    public LoginPanel() {
        initWidget(binder.createAndBindUi(this));
        String email = Cookies.getCookie("email");
        if (email != null)
            emailTextBox.setText(email);
    }

    @UiHandler("loginButton")
    void onLoginButtonClick(ClickEvent event) {

        RequestBuilder builder = new RequestBuilder(RequestBuilder.GET,
                defold.getUrl() + "/login");
        try {
            String email = emailTextBox.getText();
            String password = passwordTextBox.getText();
            String data = email + "\n" + password;

            builder.setHeader("X-Email", email);
            builder.setHeader("X-Password", password);

            builder.sendRequest(data, new RequestCallback() {
                @Override
                public void onResponseReceived(Request request,
                        Response response) {
                    int status = response.getStatusCode();
                    if (status == 200) {
                        Date now = new Date();
                        long nowLong = now.getTime();
                        nowLong = nowLong + (1000 * 60 * 60 * 24 * 7);
                        now.setTime(nowLong);

                        JSONObject loginInfo = JSONParser.parseStrict(response.getText()).isObject();

                        Cookies.setCookie("email", emailTextBox.getText(), now);
                        Cookies.setCookie("auth", loginInfo.get("auth").isString().stringValue(), now);
                        defold.loginOk(emailTextBox.getText(), (int) loginInfo.get("user_id").isNumber().doubleValue());
                        errorLabel.setVisible(false);
                        passwordTextBox.setText("");
                    } else {
                        errorLabel.setText("Login failed");
                    }
                }

                @Override
                public void onError(Request request, Throwable exception) {
                    System.out.println("ERROR!");

                }
            });
        } catch (RequestException e) {
            e.printStackTrace();
        }
    }

    public void setDefold(Defold defold) {
        this.defold = defold;
    }
}
