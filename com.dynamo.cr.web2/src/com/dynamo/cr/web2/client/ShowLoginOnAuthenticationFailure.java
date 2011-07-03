package com.dynamo.cr.web2.client;

import com.google.web.bindery.event.shared.EventBus;
import com.google.web.bindery.event.shared.HandlerRegistration;

public class ShowLoginOnAuthenticationFailure implements AuthenticationFailureEvent.Handler {

  private Defold defold;

public HandlerRegistration register(Defold defold, EventBus eventBus) {
      this.defold = defold;
    return AuthenticationFailureEvent.register(eventBus, this);
  }

  public void onAuthFailure(AuthenticationFailureEvent requestEvent) {
      defold.showLogin();
  }
}
