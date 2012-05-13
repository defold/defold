package com.dynamo.cr.web;

import org.apache.wicket.authentication.AuthenticatedWebApplication;
import org.apache.wicket.authentication.AuthenticatedWebSession;
import org.apache.wicket.markup.html.WebPage;
import org.apache.wicket.util.lang.PackageName;

import com.dynamo.cr.web.pages.HomePage;
import com.dynamo.cr.web.pages.LoginPage;
import com.dynamo.cr.web.pages.DashboardPage;

public class Application extends AuthenticatedWebApplication
{
    public Application()
    {
        mount("pages", PackageName.forClass(DashboardPage.class));
    }

    public Class<HomePage> getHomePage()
    {
        return HomePage.class;
    }

    @Override
    protected Class<? extends WebPage> getSignInPageClass() {
        return LoginPage.class;
    }

    @Override
    protected Class<? extends AuthenticatedWebSession> getWebSessionClass() {
        return Session.class;
    }
}
