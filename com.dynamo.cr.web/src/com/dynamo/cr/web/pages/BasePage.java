package com.dynamo.cr.web.pages;

import org.apache.wicket.markup.html.WebPage;
import org.apache.wicket.markup.html.link.BookmarkablePageLink;
import org.apache.wicket.markup.html.link.Link;

import com.dynamo.cr.web.Session;
import com.dynamo.cr.web.User;

public class BasePage extends WebPage {

    @SuppressWarnings("serial")
    public BasePage() {

        final Session session = (Session) getSession();
        User user = session.getUser();
        add(new BookmarkablePageLink<DashboardPage>("dashboard", DashboardPage.class));

        BookmarkablePageLink<LoginPage> login = new BookmarkablePageLink<LoginPage>("login", LoginPage.class);
        Link<HomePage> logout = new Link<HomePage>("logout") {
            @Override
            public void onClick() {
                session.invalidate();
                setResponsePage(HomePage.class);
            }
        };
        add(login);
        add(logout);

        if (user == null)
            logout.setVisible(false);
        else
            login.setVisible(false);
    }
}
