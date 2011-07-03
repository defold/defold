/**
 *
 */
package com.dynamo.cr.web2.client;

import com.google.gwt.core.client.EntryPoint;
import com.google.gwt.core.client.GWT;
import com.google.gwt.event.dom.client.ClickEvent;
import com.google.gwt.event.logical.shared.SelectionEvent;
import com.google.gwt.event.logical.shared.SelectionHandler;
import com.google.gwt.http.client.Request;
import com.google.gwt.http.client.RequestBuilder;
import com.google.gwt.http.client.RequestCallback;
import com.google.gwt.http.client.RequestException;
import com.google.gwt.http.client.Response;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.uibinder.client.UiHandler;
import com.google.gwt.user.client.Cookies;
import com.google.gwt.user.client.ui.Anchor;
import com.google.gwt.user.client.ui.DeckPanel;
import com.google.gwt.user.client.ui.DockLayoutPanel;
import com.google.gwt.user.client.ui.RootLayoutPanel;
import com.google.gwt.user.client.ui.TabLayoutPanel;
import com.google.gwt.user.client.ui.Widget;
import com.google.web.bindery.event.shared.EventBus;
import com.google.web.bindery.event.shared.SimpleEventBus;

/**
 * @author chmu
 *
 */
public class Defold implements EntryPoint {

    private static DefoldUiBinder uiBinder = GWT.create(DefoldUiBinder.class);
    private EventBus eventBus = new SimpleEventBus();

    @UiField Dashboard dashboardTab;
    @UiField TabLayoutPanel tabPanel;
    @UiField DeckPanel deckPanel;
    @UiField LoginPanel loginPanel;
    @UiField Anchor logout;
    private int userId;
    private MessageNotification messageNotification;

    interface DefoldUiBinder extends UiBinder<DockLayoutPanel, Defold> {
    }

    public Defold() {
        new ShowLoginOnAuthenticationFailure().register(this, eventBus);
        messageNotification = new MessageNotification();
        messageNotification.setPopupPosition(300, 30);
        messageNotification.setSize("120px", "30px");
        messageNotification.setAnimationEnabled(true);
    }

    void showErrorMessage(String message) {
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

        sendRequest(resource, RequestBuilder.GET, "", interceptCallback);
    }

    private void sendRequest(String resource, RequestBuilder.Method method, String requestData, final ResourceCallback<String> callback) {
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

        try {
            requestBuilder.sendRequest(requestData, new RequestCallback() {

                @Override
                public void onResponseReceived(Request request, Response response) {
                    int statusCode = response.getStatusCode();
                    if (statusCode == 401) {
                        eventBus.fireEvent(new AuthenticationFailureEvent());
                    } else if (statusCode == 0) {
                        showErrorMessage("Network error");
                        callback.onFailure(request, response);
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
        sendRequest(resource, RequestBuilder.DELETE, "", callback);
    }

    public void postResource(String resource, String data, final ResourceCallback<String> callback) {
        sendRequest(resource, RequestBuilder.POST, data, callback);
    }

    @Override
    public void onModuleLoad() {
        DockLayoutPanel outer = uiBinder.createAndBindUi(this);

        RootLayoutPanel root = RootLayoutPanel.get();
        root.add(outer);

        logout.setVisible(false);

        tabPanel.addSelectionHandler(new SelectionHandler<Integer>() {

            @Override
            public void onSelection(SelectionEvent<Integer> event) {
                Widget widget = tabPanel.getWidget(event.getSelectedItem());
                if (widget instanceof Dashboard) {
                    ((Dashboard) widget).load();
                }
            }
        });

        loginPanel.setDefold(this);
        dashboardTab.setDefold(this);

        final String email = Cookies.getCookie("email");
        if (email != null) {
            getResource("/users/" + email, new ResourceCallback<UserInfo>() {
                @Override
                public void onSuccess(UserInfo result, Request request,
                        Response response) {
                    loginOk(email, result.getId());
                }

                @Override
                public void onFailure(Request request, Response response) {
                    deckPanel.showWidget(0);
                }
            });
        } else {
            deckPanel.showWidget(0);
        }
//        deckPanel.showWidget(1);
    }

    public void loginOk(String email, int userId) {
        this.userId = userId;
        this.logout.setText("Logout " + email);
        logout.setVisible(true);
        deckPanel.showWidget(1);
    }

    public int getUserId() {
        return userId;
    }

    @UiHandler("logout")
    void onLogoutClick(ClickEvent event) {
        Cookies.removeCookie("auth");
        deckPanel.showWidget(0);
        logout.setVisible(false);
    }

    public void showLogin() {
        deckPanel.showWidget(0);
    }

    public String getUrl() {
        return "http://overrated.dyndns.org:9998";
        //return "http://127.0.0.1:9998";
    }

}
