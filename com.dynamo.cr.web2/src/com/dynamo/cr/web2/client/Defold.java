/**
 *
 */
package com.dynamo.cr.web2.client;

import com.dynamo.cr.web2.client.mvp.AppActivityMapper;
import com.dynamo.cr.web2.client.mvp.AppPlaceHistoryMapper;
import com.dynamo.cr.web2.client.place.DashboardPlace;
import com.dynamo.cr.web2.client.place.LoginPlace;
import com.dynamo.cr.web2.client.place.ProductInfoPlace;
import com.google.gwt.activity.shared.ActivityManager;
import com.google.gwt.activity.shared.ActivityMapper;
import com.google.gwt.core.client.EntryPoint;
import com.google.gwt.core.client.GWT;
import com.google.gwt.event.dom.client.ClickEvent;
import com.google.gwt.http.client.Request;
import com.google.gwt.http.client.RequestBuilder;
import com.google.gwt.http.client.RequestCallback;
import com.google.gwt.http.client.RequestException;
import com.google.gwt.http.client.Response;
import com.google.gwt.place.shared.Place;
import com.google.gwt.place.shared.PlaceController;
import com.google.gwt.place.shared.PlaceHistoryHandler;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.uibinder.client.UiHandler;
import com.google.gwt.user.client.Cookies;
import com.google.gwt.user.client.ui.Anchor;
import com.google.gwt.user.client.ui.DockLayoutPanel;
import com.google.gwt.user.client.ui.RootLayoutPanel;
import com.google.gwt.user.client.ui.SimplePanel;

/**
 * @author chmu
 *
 */
public class Defold implements EntryPoint {

    private static DefoldUiBinder uiBinder = GWT.create(DefoldUiBinder.class);

    private Place defaultPlace = new ProductInfoPlace();

    @UiField Anchor logout;
    @UiField SimplePanel panel;
    @UiField Anchor dashBoard;
    @UiField Anchor productInfo;
    private MessageNotification messageNotification;

    private com.google.gwt.event.shared.EventBus eventBus;

    private ClientFactory clientFactory;

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
                        //showErrorMessage("Network error");
                        //callback.onFailure(request, response);
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
        AppPlaceHistoryMapper historyMapper= GWT.create(AppPlaceHistoryMapper.class);
        PlaceHistoryHandler historyHandler = new PlaceHistoryHandler(historyMapper);
        historyHandler.register(placeController, eventBus, defaultPlace);

        //



        RootLayoutPanel root = RootLayoutPanel.get();
        root.add(outer);
        //root.add(outer);
        historyHandler.handleCurrentHistory();

        logout.setVisible(Cookies.getCookie("auth") != null);

        new ShowLoginOnAuthenticationFailure().register(clientFactory, eventBus);
        messageNotification = new MessageNotification();
        messageNotification.setPopupPosition(300, 30);
        messageNotification.setSize("120px", "30px");
        messageNotification.setAnimationEnabled(true);
    }

    public void loginOk(String email, int userId) {
        Cookies.setCookie("user_id", Integer.toString(userId));
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
        return "http://overrated.dyndns.org:9998";
        //return "http://127.0.0.1:9998";
    }

    @UiHandler("productInfo")
    void onProductInfoClick(ClickEvent event) {
        clientFactory.getPlaceController().goTo(new ProductInfoPlace());
    }

    @UiHandler("dashBoard")
    void onDashBoardClick(ClickEvent event) {
        clientFactory.getPlaceController().goTo(new DashboardPlace());
    }
}
