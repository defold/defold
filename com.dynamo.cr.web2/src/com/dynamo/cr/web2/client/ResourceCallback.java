package com.dynamo.cr.web2.client;

import com.google.gwt.http.client.Request;
import com.google.gwt.http.client.Response;

public interface ResourceCallback<T> {

    void onSuccess(T result, Request request, Response response);
    void onFailure(Request request, Response response);
  }
