package com.dynamo.cr.web2.client;

import com.dynamo.cr.web2.client.place.LoginPlace;
import com.google.web.bindery.event.shared.EventBus;
import com.google.web.bindery.event.shared.HandlerRegistration;

public class ShowLoginOnAuthenticationFailure implements
        AuthenticationFailureEvent.Handler {

    private ClientFactory clientFactory;

    public HandlerRegistration register(ClientFactory clientFactory,
            EventBus eventBus) {
        this.clientFactory = clientFactory;
        return AuthenticationFailureEvent.register(eventBus, this);
    }

    public void onAuthFailure(AuthenticationFailureEvent requestEvent) {
        clientFactory.getPlaceController().goTo(new LoginPlace());
    }
}
