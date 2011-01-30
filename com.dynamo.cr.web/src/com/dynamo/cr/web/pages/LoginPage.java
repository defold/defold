package com.dynamo.cr.web.pages;

import org.apache.wicket.authentication.panel.SignInPanel;

public class LoginPage extends BasePage {

    public LoginPage() {
        add(new SignInPanel("signInPanel"));
    }
}
