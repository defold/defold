package com.dynamo.cr.web.pages;

import com.dynamo.cr.web.panel.LoginPanel;

public class LoginPage extends BasePage {

    public LoginPage() {
        add(new LoginPanel("signInPanel"));
    }
}
