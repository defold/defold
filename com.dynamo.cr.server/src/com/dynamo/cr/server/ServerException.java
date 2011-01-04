package com.dynamo.cr.server;


public class ServerException extends Exception {

    public ServerException(String msg) {
        super(msg);
    }

    public ServerException(String msg, Throwable e) {
        super(msg, e);
    }

    /**
     *
     */
    private static final long serialVersionUID = 6946638416446500335L;

}
