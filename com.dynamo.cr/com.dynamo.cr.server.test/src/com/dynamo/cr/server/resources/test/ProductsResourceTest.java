package com.dynamo.cr.server.resources.test;

import static org.junit.Assert.assertEquals;

import java.net.URI;

import javax.persistence.EntityManager;
import javax.ws.rs.core.MediaType;
import javax.ws.rs.core.UriBuilder;

import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.common.providers.JsonProviders;
import com.dynamo.cr.common.providers.ProtobufProviders;
import com.dynamo.cr.protocol.proto.Protocol.ProductInfoList;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.model.User.Role;
import com.sun.jersey.api.client.Client;
import com.sun.jersey.api.client.UniformInterfaceException;
import com.sun.jersey.api.client.WebResource;
import com.sun.jersey.api.client.config.DefaultClientConfig;
import com.sun.jersey.api.client.filter.HTTPBasicAuthFilter;

public class ProductsResourceTest extends AbstractResourceTest {

    int port = 6500;
    String joeEmail = "joe@foo.com";
    String joePasswd = "secret2";
    User joeUser;
    WebResource joeUsersWebResource;
    DefaultClientConfig clientConfig;
    WebResource anonymousResource;

    @Before
    public void setUp() throws Exception {
        setupUpTest();

        EntityManager em = emf.createEntityManager();
        em.getTransaction().begin();

        joeUser = new User();
        joeUser.setEmail(joeEmail);
        joeUser.setFirstName("undefined");
        joeUser.setLastName("undefined");
        joeUser.setPassword(joePasswd);
        joeUser.setRole(Role.USER);
        em.persist(joeUser);

        em.getTransaction().commit();

        clientConfig = new DefaultClientConfig();
        clientConfig.getClasses().add(JsonProviders.ProtobufMessageBodyReader.class);
        clientConfig.getClasses().add(JsonProviders.ProtobufMessageBodyWriter.class);
        clientConfig.getClasses().add(ProtobufProviders.ProtobufMessageBodyReader.class);
        clientConfig.getClasses().add(ProtobufProviders.ProtobufMessageBodyWriter.class);

        Client client = Client.create(clientConfig);
        client.addFilter(new HTTPBasicAuthFilter(joeEmail, joePasswd));
        URI uri = UriBuilder.fromUri(String.format("http://localhost/products")).port(port).build();
        joeUsersWebResource = client.resource(uri);

        uri = UriBuilder.fromUri(String.format("http://localhost/products")).port(port).build();
        client = Client.create(clientConfig);
        anonymousResource = client.resource(uri);
    }

    @Test
    public void testGetProductsAsUser() throws Exception {
        ProductInfoList productInfoList = joeUsersWebResource.accept(MediaType.APPLICATION_JSON_TYPE)
                .type(MediaType.APPLICATION_JSON_TYPE).get(ProductInfoList.class);

        assertEquals(2, productInfoList.getProductsCount());
    }

    @Test
    public void testGetProductsByHandle() throws Exception {
        ProductInfoList productInfoList = joeUsersWebResource.queryParam("handle", "free")
                .accept(MediaType.APPLICATION_JSON_TYPE)
                .type(MediaType.APPLICATION_JSON_TYPE).get(ProductInfoList.class);

        assertEquals(1, productInfoList.getProductsCount());
    }

    @Test(expected = UniformInterfaceException.class)
    public void testGetProductsAsAnonymous() throws Exception {
        ProductInfoList productInfoList = anonymousResource.accept(MediaType.APPLICATION_JSON_TYPE)
                .type(MediaType.APPLICATION_JSON_TYPE).get(ProductInfoList.class);

        assertEquals(2, productInfoList.getProductsCount());
    }

}

