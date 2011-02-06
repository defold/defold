package com.dynamo.cr.web.pages;

import org.apache.wicket.markup.html.basic.Label;


public class WelcomeNewPage extends BasePage {

    public WelcomeNewPage(String firstName, String lastName) {
        super();
        add(new Label("firstName", firstName));
        add(new Label("lastName", lastName));
    }
}
