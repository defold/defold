package com.dynamo.cr.web2.client;

import com.dynamo.cr.web2.client.place.LoginPlace;
import com.google.gwt.place.shared.Place;
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

    @Override
    public void onAuthFailure(AuthenticationFailureEvent requestEvent) {
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
        clientFactory.getPlaceController().goTo(new LoginPlace(""));
    }
}
