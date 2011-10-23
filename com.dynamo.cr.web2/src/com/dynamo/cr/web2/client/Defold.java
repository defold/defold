/**
 *
 */
package com.dynamo.cr.web2.client;

import java.util.Date;

import com.dynamo.cr.web2.client.mvp.AppActivityMapper;
import com.dynamo.cr.web2.client.mvp.AppPlaceHistoryMapper;
import com.dynamo.cr.web2.client.place.BlogPlace;
import com.dynamo.cr.web2.client.place.DashboardPlace;
import com.dynamo.cr.web2.client.place.DocumentationPlace;
import com.dynamo.cr.web2.client.place.LoginPlace;
import com.dynamo.cr.web2.client.place.ProductInfoPlace;
import com.dynamo.cr.web2.client.place.TutorialsPlace;
import com.dynamo.cr.web2.client.ui.EditableLabel;
import com.google.gwt.activity.shared.ActivityManager;
import com.google.gwt.activity.shared.ActivityMapper;
import com.google.gwt.core.client.EntryPoint;
import com.google.gwt.core.client.GWT;
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
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.uibinder.client.UiHandler;
import com.google.gwt.user.client.Cookies;
import com.google.gwt.user.client.DOM;
import com.google.gwt.user.client.Element;
import com.google.gwt.user.client.ui.Anchor;
import com.google.gwt.user.client.ui.DockLayoutPanel;
import com.google.gwt.user.client.ui.RootLayoutPanel;
import com.google.gwt.user.client.ui.ScrollPanel;
import com.google.web.bindery.event.shared.EventBus;

/**
 * @author chmu
 *
 */
public class Defold implements EntryPoint {

    private static DefoldUiBinder uiBinder = GWT.create(DefoldUiBinder.class);

    private Place defaultPlace = new ProductInfoPlace();

    @UiField Anchor logout;
    @UiField ScrollPanel panel;
    @UiField Anchor dashBoard;
    @UiField Anchor productInfo;
    @UiField EditableLabel editableLabel;
    private MessageNotification messageNotification;

    private EventBus eventBus;

    private String url = "http://cr.defold.se:9998";

    private ClientFactory clientFactory;

    private com.google.gwt.dom.client.Element prevActiveNavElement;

    private AppPlaceHistoryMapper historyMapper;

    interface DefoldUiBinder extends UiBinder<DockLayoutPanel, Defold> {
    }

    public Defold() {
    }

    public void showErrorMessage(String message) {
        messageNotification.show(message);
    }

    public <T extends BaseResponse> void getResource(String resource, final ResourceCallback<T> callback) {

        ResourceCallback<String> interceptCallback = new ResourceCallback<String>() {

            @SuppressWarnings("unchecked")
            @Override
            public void onSuccess(String result, Request request, Response response) {
                callback.onSuccess((T) BaseResponse.getResponse(response.getText()), request, response);
            }

            @Override
            public void onFailure(Request request, Response response) {
                callback.onFailure(request, response);
            }
        };

        sendRequest(resource, RequestBuilder.GET, "", interceptCallback, null);
    }

    private void sendRequest(String resource, RequestBuilder.Method method, String requestData, final ResourceCallback<String> callback, String contentType) {
        String url = getUrl() + resource;
        RequestBuilder requestBuilder = new RequestBuilder(method, url);
        String email = Cookies.getCookie("email");
        String auth = Cookies.getCookie("auth");
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

    public void putResource(String resource, String data, final ResourceCallback<String> callback) {
        sendRequest(resource, RequestBuilder.PUT, data, callback, "application/json");
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
                $wnd._gaq.push(['_setAccount', 'UA-83690-3']);
                $wnd._gaq.push(['_setDomainName', 'www.defold.se']);
                $wnd._gaq.push(['_trackPageview', pageName]);
            } catch(err) {
            }
        }-*/;

        @Override
        public void onPlaceChange(PlaceChangeEvent event) {
            String token = historyMapper.getToken(event.getNewPlace());
            trackHit(token);
        }
    }

    @Override
    public void onModuleLoad() {

        DockLayoutPanel outer = uiBinder.createAndBindUi(this);

        clientFactory = GWT.create(ClientFactory.class);
        clientFactory.setDefold(this);
        eventBus = clientFactory.getEventBus();
        PlaceController placeController = clientFactory.getPlaceController();

        // Start ActivityManager for the main widget with our ActivityMapper
        ActivityMapper activityMapper = new AppActivityMapper(clientFactory);
        ActivityManager activityManager = new ActivityManager(activityMapper, eventBus);
        activityManager.setDisplay(panel);

        // Start PlaceHistoryHandler with our PlaceHistoryMapper
        historyMapper = GWT.create(AppPlaceHistoryMapper.class);
        // NOTE: We must add this early in order to catch "first page"
        eventBus.addHandler(PlaceChangeEvent.TYPE, new NavigationHandler());

        // Add google analytics handler
        eventBus.addHandler(PlaceChangeEvent.TYPE, new GoogleAnalyticsHandler());

        PlaceHistoryHandler historyHandler = new PlaceHistoryHandler(historyMapper);
        historyHandler.register(placeController, eventBus, defaultPlace);

        String url = Cookies.getCookie("url");
        if (url != null) {
            this.url = url;
        }
        editableLabel.setValue(this.url);

        RootLayoutPanel root = RootLayoutPanel.get();
        root.add(outer);
        historyHandler.handleCurrentHistory();

        if (Cookies.getCookie("auth") != null && Cookies.getCookie("email") != null) {
            logout.setVisible(Cookies.getCookie("auth") != null);
            logout.setText("Logout (" + Cookies.getCookie("email") + ")");
        }

        new ShowLoginOnAuthenticationFailure().register(clientFactory, eventBus);
        messageNotification = new MessageNotification();
        messageNotification.setPopupPosition(300, 30);
        messageNotification.setSize("120px", "30px");
        messageNotification.setAnimationEnabled(true);
    }

    public void loginOk(String email, String authCookie, int userId) {
        Date expires = new Date();
        long nowLong = expires.getTime();
        nowLong = nowLong + (1000 * 60 * 60 * 24 * 7);
        expires.setTime(nowLong);

        Cookies.setCookie("user_id", Integer.toString(userId), expires);
        Cookies.setCookie("email", email, expires);
        Cookies.setCookie("auth", authCookie, expires);

        logout.setText("Logout (" + email + ")");
        logout.setVisible(true);
    }

    public int getUserId() {
        String userId = Cookies.getCookie("user_id");
        if (userId == null) {
            clientFactory.getPlaceController().goTo(new LoginPlace());
            return -1;
        } else {
            return Integer.parseInt(userId);
        }
    }

    @UiHandler("logout")
    void onLogoutClick(ClickEvent event) {
        Cookies.removeCookie("auth");
        logout.setVisible(false);
        clientFactory.getPlaceController().goTo(new ProductInfoPlace());
    }

    public void showLogin() {
        //deckPanel.showWidget(0);
    }

    public String getUrl() {
        return url;
    }

    @UiHandler("productInfo")
    void onProductInfoClick(ClickEvent event) {
        clientFactory.getPlaceController().goTo(new ProductInfoPlace());
    }

    @UiHandler("dashBoard")
    void onDashBoardClick(ClickEvent event) {
        clientFactory.getPlaceController().goTo(new DashboardPlace());
    }

    @UiHandler("documentation")
    void onDocumentationClick(ClickEvent event) {
        clientFactory.getPlaceController().goTo(new DocumentationPlace());
    }

    @UiHandler("tutorials")
    void onTutorialsClick(ClickEvent event) {
        clientFactory.getPlaceController().goTo(new TutorialsPlace(""));
    }

    @UiHandler("blog")
    void onBlogClick(ClickEvent event) {
        clientFactory.getPlaceController().goTo(new BlogPlace());
    }

    @UiHandler("editableLabel")
    void onEditableLabelValueChange(ValueChangeEvent<String> event) {
        this.url = event.getValue();
        Date expires = new Date();
        long nowLong = expires.getTime();
        nowLong = nowLong + (1000 * 60 * 60 * 24 * 100);
        expires.setTime(nowLong);

        Cookies.setCookie("url", url, expires);
    }
}
