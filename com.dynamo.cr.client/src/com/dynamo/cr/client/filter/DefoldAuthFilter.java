package com.dynamo.cr.client.filter;

import java.util.List;

import javax.ws.rs.core.MultivaluedMap;
import javax.ws.rs.core.NewCookie;

import com.sun.jersey.api.client.ClientHandlerException;
import com.sun.jersey.api.client.ClientRequest;
import com.sun.jersey.api.client.ClientResponse;
import com.sun.jersey.api.client.filter.ClientFilter;


/**
 * Client filter for custom defold authentication scheme. X-Auth, X-Email and X-Password headers
 * can be added to the HTTP request. The filter is able to snoop for auth-cookies returned from login requests.
 * If the authCookie is known password can be left out. If the authCookie isn't known password can be used instead and the authCookie will be snooped.
 * If such cookie is found authCookie is replaced by the value of the new cookie and the password is not longer sent.
 * See constructor for more information.
 * @author chmu
 *
 */
public class DefoldAuthFilter extends ClientFilter {

    private String authCookie;
    private String email;
    private String password;

    /**
     * Creates a new filter using email, authCookie and password and sets corresponding HTTP headers. email is mandatory but
     * authCookie and password are only set if not null.
     *
     * @param email email to set. Can not be null.
     * @param authCookie authCookie. Optional argument.
     * @param password password to set. Optional argument.
     */
    public DefoldAuthFilter(String email, String authCookie, String password) {
        this.email = email;
        this.authCookie = authCookie;
        this.password = password;
    }

    @Override
    public ClientResponse handle(ClientRequest cr) throws ClientHandlerException {
        MultivaluedMap<String, Object> headers = cr.getHeaders();
        if (this.authCookie != null) {
            headers.add("X-Auth", this.authCookie);
        }

        headers.add("X-Email", this.email);

        // Send only password if password is set and authCookie is not set.
        if (this.password != null && authCookie == null) {
            headers.add("X-Password", this.password);
        }

        ClientResponse response = getNext().handle(cr);
        List<NewCookie> cookies = response.getCookies();
        for (NewCookie newCookie : cookies) {
            if (newCookie.getName().equals("auth")) {
                // Snoop auth-cookie
                this.authCookie = newCookie.getValue();
            }
        }
        return response;
    }

    public void setEmail(String email) {
        this.email = email;
    }

    public void setAuthCookie(String authCookie) {
        this.authCookie = authCookie;
    }

}
