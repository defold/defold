package com.dynamo.cr.web2.client;

import com.google.gwt.event.shared.EventHandler;
import com.google.gwt.event.shared.GwtEvent;
import com.google.web.bindery.event.shared.EventBus;
import com.google.web.bindery.event.shared.HandlerRegistration;

public class AuthenticationFailureEvent extends GwtEvent<AuthenticationFailureEvent.Handler>  {

    /**
     * Implemented by handlers of this type of event.
     */
    public interface Handler extends EventHandler {
      /**
       * Called when a {@link AuthenticationFailureEvent} is fired.
       *
       * @param requestEvent a {@link AuthenticationFailureEvent} instance
       */
      void onAuthFailure(AuthenticationFailureEvent requestEvent);
    }

    private static final Type<Handler> TYPE = new Type<Handler>();

    /**
     * Register a {@link AuthenticationFailureEvent.Handler} on an {@link EventBus}.
     *
     * @param eventBus the {@link EventBus}
     * @param handler a {@link AuthenticationFailureEvent.Handler}
     * @return a {@link HandlerRegistration} instance
     */
    public static HandlerRegistration register(EventBus eventBus,
        AuthenticationFailureEvent.Handler handler) {
      return eventBus.addHandler(TYPE, handler);
    }

    @Override
    public com.google.gwt.event.shared.GwtEvent.Type<Handler> getAssociatedType() {
        return TYPE;
    }

    @Override
    protected void dispatch(Handler handler) {
        handler.onAuthFailure(this);
    }

}
