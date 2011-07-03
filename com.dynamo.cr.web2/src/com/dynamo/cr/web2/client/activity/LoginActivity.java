package com.dynamo.cr.web2.client.activity;

import java.util.Date;

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
import com.google.gwt.json.client.JSONObject;
import com.google.gwt.json.client.JSONParser;
import com.google.gwt.user.client.Cookies;
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
                        Date now = new Date();
                        long nowLong = now.getTime();
                        nowLong = nowLong + (1000 * 60 * 60 * 24 * 7);
                        now.setTime(nowLong);

                        JSONObject loginInfo = JSONParser.parseStrict(response.getText()).isObject();

                        Cookies.setCookie("email", email, now);
                        Cookies.setCookie("auth", loginInfo.get("auth").isString().stringValue(), now);
                        defold.loginOk(email, (int) loginInfo.get("user_id").isNumber().doubleValue());
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
}
