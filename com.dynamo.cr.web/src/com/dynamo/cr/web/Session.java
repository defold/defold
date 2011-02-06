package com.dynamo.cr.web;

import java.net.URI;

import javax.ws.rs.core.UriBuilder;

import org.apache.wicket.Request;
import org.apache.wicket.authentication.AuthenticatedWebSession;
import org.apache.wicket.authorization.strategies.role.Roles;

import com.dynamo.cr.protocol.proto.Protocol.UserInfo;
import com.dynamo.cr.web.providers.ProtobufProviders;
import com.sun.jersey.api.client.Client;
import com.sun.jersey.api.client.ClientResponse;
import com.sun.jersey.api.client.WebResource;
import com.sun.jersey.api.client.config.ClientConfig;
import com.sun.jersey.api.client.config.DefaultClientConfig;
import com.sun.jersey.api.client.filter.HTTPBasicAuthFilter;

public class Session extends AuthenticatedWebSession {

    private static final long serialVersionUID = -3573895895174366629L;
    private User user = null;
    private int servicePort = 9998;

    public Session(Request request) {
        super(request);
        String servicePort = Application.get().getServletContext().getInitParameter("service-port");
        if (servicePort != null) {
            this.servicePort = Integer.parseInt(servicePort);
        }
    }

    @Override
    public boolean authenticate(String username, String password) {
        ClientConfig cc = createClientConfig();

        Client client = Client.create(cc);
        client.addFilter(new HTTPBasicAuthFilter(username, password));

        URI uri = UriBuilder.fromUri("http://localhost").port(servicePort).build();
        WebResource resource = client.resource(uri);
        ClientResponse ret = resource.path("/users/" + username)
                                .accept(ProtobufProviders.APPLICATION_XPROTOBUF)
                                .get(ClientResponse.class);

        if (ret.getStatus() == 200) {

            UserInfo userInfo = ret.getEntity(UserInfo.class);
            user = new User(userInfo.getId(), username, password, uri);
            return true;
        }
        return false;
    }

    private ClientConfig createClientConfig() {
        ClientConfig cc = new DefaultClientConfig();
        cc.getClasses().add(ProtobufProviders.ProtobufMessageBodyReader.class);
        cc.getClasses().add(ProtobufProviders.ProtobufMessageBodyWriter.class);
        return cc;
    }

    @Override
    public Roles getRoles() {
        if (isSignedIn()) {
            return new Roles(Roles.USER);
        }
        return null;
    }

    public User getUser() {
        return user;
    }

    public WebResource getWebResource() {
        ClientConfig cc = createClientConfig();
        Client client = Client.create(cc);
        client.addFilter(new HTTPBasicAuthFilter(user.getUser(), user.getPassword()));
        URI uri = UriBuilder.fromUri("http://localhost").port(servicePort).build();
        return client.resource(uri);
    }

    public WebResource getAdminWebResource() {
        ClientConfig cc = createClientConfig();
        Client client = Client.create(cc);
        client.addFilter(new HTTPBasicAuthFilter("dynamogameengine@gmail.com", "mobEpeyfUj9"));
        URI uri = UriBuilder.fromUri("http://localhost").port(this.servicePort).build();
        return client.resource(uri);
    }

}
