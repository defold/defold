package com.dynamo.cr.server.openid;

public class OpenIDException extends Exception {

    /**
    *
    */
   private static final long serialVersionUID = 1L;

    public OpenIDException(String message) {
        super(message);
    }

    public OpenIDException(Throwable e) {
        super(e);
    }


}
