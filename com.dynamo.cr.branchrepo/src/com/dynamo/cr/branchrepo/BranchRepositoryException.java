package com.dynamo.cr.branchrepo;

import javax.ws.rs.core.Response.Status;


public class BranchRepositoryException extends Exception {

    private Status status = Status.BAD_REQUEST;

    public BranchRepositoryException(String msg) {
        super(msg);
    }

    public BranchRepositoryException(String msg, Throwable e) {
        super(msg, e);
    }

    public BranchRepositoryException(String msg, Status status) {
        super(msg);
        this.status = status;
    }

    public BranchRepositoryException(String msg, Throwable e, Status status) {
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
