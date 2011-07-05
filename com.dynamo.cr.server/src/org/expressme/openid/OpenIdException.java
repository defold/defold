package org.expressme.openid;

/**
 * Exception for any open id authentication.
 * 
 * @author Michael Liao (askxuefeng@gmail.com)
 */
public class OpenIdException extends RuntimeException {

    /**
     * OpenIdException with no message.
     */
    public OpenIdException() {
        super();
    }

    /**
     * OpenIdException with message and cause.
     */
    public OpenIdException(String message, Throwable cause) {
        super(message, cause);
    }

    /**
     * OpenIdException with message.
     */

    public OpenIdException(String message) {
        super(message);
    }

    /**
     * OpenIdException with cause.
     */
    public OpenIdException(Throwable cause) {
        super(cause);
    }

}
