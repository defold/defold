package com.dynamo.cr.server;

import javax.ws.rs.core.Response.Status;

/*
 * NOTE: ServerException must be a subclass of RuntimeException
 * as transactions are only rollbacked by default for RuntimeExceptions
 */
public class ServerException extends RuntimeException {

    private Status status = Status.BAD_REQUEST;

    public ServerException(String msg) {
        super(msg);
    }

    public ServerException(String msg, Throwable e) {
        super(msg, e);
    }

    public ServerException(String msg, Status status) {
        super(msg);
        this.status = status;
    }

    public ServerException(String msg, Throwable e, Status status) {
        super(msg, e);
        this.status = status;
    }

    public Status getStatus() {
        return status;
    }

    /**
     *
     */
    private static final long serialVersionUID = 6946638416446500335L;

}
