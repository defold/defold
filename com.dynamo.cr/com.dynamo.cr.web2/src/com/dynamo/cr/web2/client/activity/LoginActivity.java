package com.dynamo.cr.web2.client.activity;

import java.util.Date;

import com.dynamo.cr.web2.client.ClientFactory;
import com.dynamo.cr.web2.client.Defold;
import com.dynamo.cr.web2.client.place.LoginPlace;
import com.dynamo.cr.web2.client.ui.LoginView;
import com.google.gwt.activity.shared.AbstractActivity;
import com.google.gwt.event.shared.EventBus;
import com.google.gwt.http.client.URL;
import com.google.gwt.user.client.Window;
import com.google.gwt.user.client.ui.AcceptsOneWidget;

public class LoginActivity extends AbstractActivity implements LoginView.Presenter {
	private ClientFactory clientFactory;

	public LoginActivity(LoginPlace place, ClientFactory clientFactory) {
		this.clientFactory = clientFactory;

        Date expires = new Date();
        long nowLong = expires.getTime();
        nowLong = nowLong + (1000 * 60 * 30);
        expires.setTime(nowLong);
        Defold.setCookie("registration_key", place.getRegistrationKey(), expires);
    }

	@Override
	public void start(AcceptsOneWidget containerWidget, EventBus eventBus) {
		LoginView loginView = clientFactory.getLoginView();
		containerWidget.setWidget(loginView.asWidget());
		loginView.setPresenter(this);
	}

	private void loginGeneric(String provider) {
        String url = clientFactory.getDefold().getUrl();

        // The redirectToUrl is the #oauth-activity, ie the url redirected to after login
        String redirectToUrl = Window.Location.createUrlBuilder().buildString();
        if (redirectToUrl.lastIndexOf('#') != -1) {
            redirectToUrl = redirectToUrl.substring(0, redirectToUrl.lastIndexOf('#'));
        }
        redirectToUrl += "#oauth:{token}_{action}";
        String openAuthUrl = url + "/login/oauth/" + provider + "?redirect_to=" + URL.encodeQueryString(redirectToUrl);
        Window.Location.replace(openAuthUrl);
	}

    @Override
    public void loginGoogle() {
        loginGeneric("google");
    }

    @Override
    public void loginYahoo() {
        loginGeneric("yahoo");
    }

}
