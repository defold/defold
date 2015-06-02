/**
 *
 */
package com.dynamo.cr.web2.client;

import java.util.Date;

import com.dynamo.cr.web2.client.mvp.AppActivityMapper;
import com.dynamo.cr.web2.client.mvp.AppPlaceHistoryMapper;
import com.dynamo.cr.web2.client.place.DashboardPlace;
import com.dynamo.cr.web2.client.place.DefoldPlace;
import com.dynamo.cr.web2.client.place.LoginPlace;
import com.dynamo.cr.web2.client.ui.BrowserWarningDialog;
import com.dynamo.cr.web2.client.ui.EditableLabel;
import com.dynamo.cr.web2.shared.ClientUtil;
import com.dynamo.cr.web2.shared.ClientUtil.Browser;
import com.dynamo.cr.web2.shared.Configuration;
import com.google.gwt.activity.shared.ActivityManager;
import com.google.gwt.activity.shared.ActivityMapper;
import com.google.gwt.core.client.EntryPoint;
import com.google.gwt.core.client.GWT;
import com.google.gwt.dom.client.Document;
import com.google.gwt.event.dom.client.ClickEvent;
import com.google.gwt.event.logical.shared.ValueChangeEvent;
import com.google.gwt.http.client.Request;
import com.google.gwt.http.client.RequestBuilder;
import com.google.gwt.http.client.RequestCallback;
import com.google.gwt.http.client.RequestException;
import com.google.gwt.http.client.Response;
import com.google.gwt.place.shared.Place;
import com.google.gwt.place.shared.PlaceChangeEvent;
import com.google.gwt.place.shared.PlaceController;
import com.google.gwt.place.shared.PlaceHistoryHandler;
import com.google.gwt.place.shared.PlaceHistoryMapper;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.uibinder.client.UiHandler;
import com.google.gwt.user.client.Cookies;
import com.google.gwt.user.client.DOM;
import com.google.gwt.user.client.Element;
import com.google.gwt.user.client.Window;
import com.google.gwt.user.client.ui.Anchor;
import com.google.gwt.user.client.ui.HTMLPanel;
import com.google.gwt.user.client.ui.RootPanel;
import com.google.gwt.user.client.ui.SimplePanel;
import com.google.web.bindery.event.shared.EventBus;

/**
 * @author chmu
 *
 */
public class Defold implements EntryPoint {

    private static DefoldUiBinder uiBinder = GWT.create(DefoldUiBinder.class);

    private Place defaultPlace = new DashboardPlace();

    @UiField Anchor logout;
    @UiField SimplePanel panel;
    @UiField EditableLabel editableLabel;
    private MessageNotification messageNotification;

    private EventBus eventBus;

    private String url = Configuration.url;

    private ClientFactory clientFactory;

    private com.google.gwt.dom.client.Element prevActiveNavElement;

    private PlaceHistoryMapper historyMapper;

    interface DefoldUiBinder extends UiBinder<HTMLPanel, Defold> {
    }

    public Defold() {
    }

    public void showErrorMessage(String message) {
        messageNotification.show(message);
    }

    public <T extends BaseResponse> void getResource(String resource, final ResourceCallback<T> callback) {
        sendRequestRetrieve(resource, RequestBuilder.GET, "", callback, null);
    }

    public void getStringResource(String resource, final ResourceCallback<String> callback) {
        sendRequestRetrieveString(resource, RequestBuilder.GET, "", callback, null);
    }

    private <T extends BaseResponse> void sendRequestRetrieve(String resource, RequestBuilder.Method method,
            String requestData,
            final ResourceCallback<T> callback, String contentType) {
        ResourceCallback<String> interceptCallback = new ResourceCallback<String>() {

            @SuppressWarnings("unchecked")
            @Override
            public void onSuccess(String result, Request request, Response response) {
                int statusCode = response.getStatusCode();
                T entity = null;
                if (statusCode >= 200 && statusCode < 300) {
                    entity = (T) BaseResponse.getResponse(response.getText());
                }
                callback.onSuccess(entity, request, response);
            }

            @Override
            public void onFailure(Request request, Response response) {
                callback.onFailure(request, response);
            }
        };

        sendRequest(resource, method, requestData, interceptCallback, contentType);
    }

    private void sendRequestRetrieveString(String resource, RequestBuilder.Method method,
                                           String requestData,
                                           final ResourceCallback<String> callback,
                                           String contentType) {
        ResourceCallback<String> interceptCallback = new ResourceCallback<String>() {

            @Override
            public void onSuccess(String result, Request request, Response response) {
                int statusCode = response.getStatusCode();
                String entity = null;
                if (statusCode >= 200 && statusCode < 300) {
                    entity = response.getText();
                }
                callback.onSuccess(entity, request, response);
            }

            @Override
            public void onFailure(Request request, Response response) {
                callback.onFailure(request, response);
            }
        };

        sendRequest(resource, method, requestData, interceptCallback, contentType);
    }

    /**
     * Set cookie under path "/". Use this in favor for Cookies.setCookie which
     * sets cookie under "curretn" path
     * @param name cookie name
     * @param value cookie value
     * @param expires expires
     */
    public static void setCookie(String name, String value, Date expires) {
        String domain = Window.Location.getHostName();
        if (domain.startsWith("www.")) {
            domain = domain.substring(3);
        }
        Cookies.setCookie(name, value, expires, domain, "/", false);
    }

    /**
     * Set cookie under path "/". Use this in favor for Cookies.setCookie which
     * sets cookie under "curretn" path
     * @param name cookie name
     * @param value cookie value
     */
    public static void setCookie(String name, String value) {
        setCookie(name, value, null);
    }

    /**
     * Remove cookie under "/"
     * @param name cookie nake
     */
    public static void removeCookie(String name) {
        setCookie(name, "", new Date(1));
    }

    public static String getCookie(String name) {
        return Cookies.getCookie(name);
    }

    private void sendRequest(String resource, RequestBuilder.Method method, String requestData, final ResourceCallback<String> callback, String contentType) {
        String url = getUrl() + resource;
        RequestBuilder requestBuilder = new RequestBuilder(method, url);
        String email = getCookie("email");
        String auth = getCookie("auth");
        if (email == null || auth == null) {
            eventBus.fireEvent(new AuthenticationFailureEvent());
            return;
        }

        requestBuilder.setHeader("X-Email", email);
        requestBuilder.setHeader("X-Auth", auth);
        requestBuilder.setHeader("Accept", "application/json");
        if (contentType != null) {
            requestBuilder.setHeader("Content-Type", contentType);
        }

        try {
            requestBuilder.sendRequest(requestData, new RequestCallback() {

                @Override
                public void onResponseReceived(Request request, Response response) {
                    int statusCode = response.getStatusCode();
                    if (statusCode == 401) {
                        eventBus.fireEvent(new AuthenticationFailureEvent());
                    } else if (statusCode == 0) {
                        eventBus.fireEvent(new AuthenticationFailureEvent());
                    }
                    else {
                        callback.onSuccess(response.getText(), request, response);
                    }
                }

                @Override
                public void onError(Request request, Throwable exception) {
                    // NOTE: We don't pass this one through. Errors related to timeout etc
                    showErrorMessage("Network error");
                }
            });
        } catch (RequestException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
        }
    }

    public void deleteResource(String resource, final ResourceCallback<String> callback) {
        sendRequest(resource, RequestBuilder.DELETE, "", callback, null);
    }

    public void postResource(String resource, String data, final ResourceCallback<String> callback) {
        sendRequest(resource, RequestBuilder.POST, data, callback, "application/json");
    }

    public <T extends BaseResponse> void postResourceRetrieve(String resource, String data,
            final ResourceCallback<T> callback) {
        sendRequestRetrieve(resource, RequestBuilder.POST, data, callback, "application/json");
    }

    public void putResource(String resource, String data, final ResourceCallback<String> callback) {
        sendRequest(resource, RequestBuilder.PUT, data, callback, "application/json");
    }

    public <T extends BaseResponse> void putResourceRetrieve(String resource, String data,
            final ResourceCallback<T> callback) {
        sendRequestRetrieve(resource, RequestBuilder.PUT, data, callback, "application/json");
    }

    /**
     * Handles activation of top-level navigation menu elements
     * @author chmu
     *
     */
    class NavigationHandler implements PlaceChangeEvent.Handler {

        @Override
        public void onPlaceChange(PlaceChangeEvent event) {
            String token = historyMapper.getToken(event.getNewPlace());
            int i = token.indexOf(":");
            token = token.substring(0, i);
            Element liElement = DOM.getElementById(token);

            if (liElement != null) {
                com.google.gwt.dom.client.Element anchorElement = liElement.getFirstChildElement();
                anchorElement.addClassName("nav1Active");
                if (anchorElement == prevActiveNavElement)
                    return;

                if (prevActiveNavElement != null) {
                    prevActiveNavElement.removeClassName("nav1Active");
                }
                prevActiveNavElement = anchorElement;

            }
        }
    }

    //  NOTE: The account id is duplicated in Defold.html
    class GoogleAnalyticsHandler implements PlaceChangeEvent.Handler {

        private native void trackHit(String pageName) /*-{
			try {
				$wnd._gaq.push([ '_setAccount', 'UA-83690-3' ]);
				$wnd._gaq.push([ '_trackPageview', pageName ]);
			} catch (err) {
			}
        }-*/;

        @Override
        public void onPlaceChange(PlaceChangeEvent event) {
            Place place = event.getNewPlace();
            if (place instanceof DefoldPlace) {
                DefoldPlace defoldPlace = (DefoldPlace) place;
                Document.get().setTitle("Defold - " + defoldPlace.getTitle());
            } else {
                Document.get().setTitle("Defold");
            }
            String token = historyMapper.getToken(place);
            if (!ClientUtil.isDev()) {
                if (token.startsWith("!")) {
                    token = token.substring(1);
                }
                trackHit(token);
            }
        }
    }

    @Override
    public void onModuleLoad() {

        HTMLPanel outer = uiBinder.createAndBindUi(this);

        clientFactory = GWT.create(ClientFactory.class);
        clientFactory.setDefold(this);
        eventBus = clientFactory.getEventBus();
        PlaceController placeController = clientFactory.getPlaceController();

        // Start ActivityManager for the main widget with our ActivityMapper
        ActivityMapper activityMapper = new AppActivityMapper(clientFactory);
        ActivityManager activityManager = new ActivityManager(activityMapper, eventBus);
        activityManager.setDisplay(panel);

        // Start PlaceHistoryHandler with our PlaceHistoryMapper
        historyMapper = new FilterPlaceHistoryMapper((PlaceHistoryMapper) GWT.create(AppPlaceHistoryMapper.class));
        // NOTE: We must add this early in order to catch "first page"
        eventBus.addHandler(PlaceChangeEvent.TYPE, new NavigationHandler());

        // Add google analytics handler
        eventBus.addHandler(PlaceChangeEvent.TYPE, new GoogleAnalyticsHandler());

        PlaceHistoryHandler historyHandler = new PlaceHistoryHandler(historyMapper);
        historyHandler.register(placeController, eventBus, defaultPlace);

        if (ClientUtil.isDev()) {
            String url = getCookie("url");
            if (url != null) {
                this.url = url;
            }
            editableLabel.setValue(this.url);
        } else {
            editableLabel.setVisible(false);
        }

        // RootPanel or RootLayoutPanel?
        RootPanel.get().add(outer);
        historyHandler.handleCurrentHistory();

        if (getCookie("auth") != null && getCookie("email") != null) {
            logout.setVisible(getCookie("auth") != null);
            logout.setText("Logout (" + getCookie("email") + ")");
        } else {
            logout.setVisible(false);
        }

        new ShowLoginOnAuthenticationFailure().register(clientFactory, eventBus);
        messageNotification = new MessageNotification();
        messageNotification.setStyleName("message");

        double version = ClientUtil.getBrowserVersion();
        Browser browser = ClientUtil.getBrowser();
        boolean supported = false;
        switch (browser) {
            case Chrome:
                supported = version >= 12;
                break;

            case Safari:
                supported = version >= 5;
                break;

            case Firefox:
                supported = version >= 5;
                break;

            case DefoldCrawler:
                supported = version >= 1;
                break;
        }

        if (!supported) {
            BrowserWarningDialog dialog = new BrowserWarningDialog();
            dialog.center();
            dialog.show();
        }
    }

    public void loginOk(String firstName, String lastName, String email, String authToken, int userId) {
        Date expires = new Date();
        long nowLong = expires.getTime();
        nowLong = nowLong + (1000 * 60 * 60 * 24 * 7);
        expires.setTime(nowLong);

        setCookie("first_name", firstName, expires);
        setCookie("last_name", lastName, expires);
        setCookie("user_id", Integer.toString(userId), expires);
        setCookie("email", email, expires);
        setCookie("auth", authToken, expires);

        logout.setText("Logout (" + email + ")");
        logout.setVisible(true);
    }

    public void setRedirectAfterLoginToken() {
        Place currentPlace = clientFactory.getPlaceController().getWhere();
        /*
         * Certain activities issues several request in parallell.
         * In such cases we are already at LoginPlace after first failing request.
         * We want to redirect to the original page. Thats why we check for LoginPlace here.
         */
        if (!(currentPlace instanceof LoginPlace)) {
            String token = clientFactory.getDefold().getHistoryMapper().getToken(currentPlace);
            Defold.setCookie("afterLoginPlaceToken", token);
        }
    }

    public int getUserId() {
        String userId = getCookie("user_id");
        if (userId == null) {
            // Another option would perhaps be to throw AuthenticationFailureEvent
            setRedirectAfterLoginToken();
            clientFactory.getPlaceController().goTo(new LoginPlace(""));
            return -1;
        } else {
            return Integer.parseInt(userId);
        }
    }

    public String getFirstName() {
        String firstName = getCookie("first_name");
        return firstName;
    }

    public String getLastName() {
        String lastName = getCookie("last_name");
        return lastName;
    }

    public String getEmail() {
        String email = getCookie("email");
        return email;
    }

    public String getRegistrationKey() {
        String registrationKey = getCookie("registration_key");
        if (registrationKey == null) {
            clientFactory.getPlaceController().goTo(new LoginPlace(""));
            showErrorMessage("Your session expired. Please click the registration link again.");
            return null;
        } else {
            return registrationKey;
        }
    }

    @UiHandler("logout")
    void onLogoutClick(ClickEvent event) {
        removeCookie("first_name");
        removeCookie("last_name");
        removeCookie("user_id");
        removeCookie("email");
        removeCookie("auth");
        removeCookie("registration_key");
        logout.setVisible(false);
        Window.open("/", "_self", "");
    }

    public void showLogin() {
        //deckPanel.showWidget(0);
    }

    public PlaceHistoryMapper getHistoryMapper() {
        return historyMapper;
    }

    public String getUrl() {
        return url;
    }

    @UiHandler("editableLabel")
    void onEditableLabelValueChange(ValueChangeEvent<String> event) {
        this.url = event.getValue();
        Date expires = new Date();
        long nowLong = expires.getTime();
        nowLong = nowLong + (1000 * 60 * 60 * 24 * 100);
        expires.setTime(nowLong);

        setCookie("url", url, expires);
    }
}

