package com.dynamo.cr.web2.client.activity;

import com.dynamo.cr.web2.client.ClientFactory;
import com.dynamo.cr.web2.client.Defold;
import com.dynamo.cr.web2.client.place.LoginPlace;
import com.dynamo.cr.web2.client.place.ProductInfoPlace;
import com.dynamo.cr.web2.client.ui.LoginView;
import com.google.gwt.activity.shared.AbstractActivity;
import com.google.gwt.event.shared.EventBus;
import com.google.gwt.http.client.Request;
import com.google.gwt.http.client.RequestBuilder;
import com.google.gwt.http.client.RequestCallback;
import com.google.gwt.http.client.RequestException;
import com.google.gwt.http.client.Response;
import com.google.gwt.http.client.URL;
import com.google.gwt.json.client.JSONObject;
import com.google.gwt.json.client.JSONParser;
import com.google.gwt.user.client.Window;
import com.google.gwt.user.client.ui.AcceptsOneWidget;

public class LoginActivity extends AbstractActivity implements LoginView.Presenter {
	private ClientFactory clientFactory;

	public LoginActivity(LoginPlace place, ClientFactory clientFactory) {
		this.clientFactory = clientFactory;
	}

	@Override
	public void start(AcceptsOneWidget containerWidget, EventBus eventBus) {
		LoginView loginView = clientFactory.getLoginView();
		containerWidget.setWidget(loginView.asWidget());
		loginView.setPresenter(this);
	}

	/*
	 * NOTE: Old login-scheme using username/password. This is obsolete and currently not supported.
	 * Only OpenID is supported.
	 */
    public void login(final String email, String password) {
        final Defold defold = clientFactory.getDefold();
        final LoginView loginView = clientFactory.getLoginView();

        RequestBuilder builder = new RequestBuilder(RequestBuilder.GET,
                defold.getUrl() + "/login");
        try {
            builder.setHeader("X-Email", email);
            builder.setHeader("X-Password", password);

            builder.sendRequest("", new RequestCallback() {
                @Override
                public void onResponseReceived(Request request,
                        Response response) {
                    int status = response.getStatusCode();
                    if (status == 200) {
                        JSONObject loginInfo = JSONParser.parseStrict(response.getText()).isObject();
                        defold.loginOk(email, loginInfo.get("auth_cookie").isString().stringValue(), (int) loginInfo.get("user_id").isNumber().doubleValue());
                        loginView.setError(null);
                        clientFactory.getPlaceController().goTo(new ProductInfoPlace());
                    } else {
                        loginView.setError("Login failed");
                    }
                }

                @Override
                public void onError(Request request, Throwable exception) {
                }
            });
        } catch (RequestException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void loginGoogle() {
        String url = clientFactory.getDefold().getUrl();
        String redirectToUrl = Window.Location.createUrlBuilder().buildString();
        if (redirectToUrl.lastIndexOf('#') != -1) {
            redirectToUrl = redirectToUrl.substring(0, redirectToUrl.lastIndexOf('#'));
        }
        redirectToUrl += "#OpenIDPlace:{token}_{action}";
        String openAuthUrl = url + "/login/openid/google?redirect_to=" + URL.encodeQueryString(redirectToUrl);
        Window.Location.replace(openAuthUrl);
    }
}
