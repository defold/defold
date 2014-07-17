package com.dynamo.cr.server.resources.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;

import java.util.Arrays;
import java.util.HashSet;
import java.util.List;

import javax.persistence.EntityManager;

import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.protocol.proto.Protocol;
import com.dynamo.cr.protocol.proto.Protocol.NewsSubscriberList;
import com.dynamo.cr.server.model.NewsSubscriber;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.model.User.Role;
import com.sun.jersey.api.client.ClientResponse;
import com.sun.jersey.api.client.WebResource;


public class NewsListResourceTest extends AbstractResourceTest {

    private WebResource adminResource;
    private WebResource bobResource;
    private WebResource joeResource;
    private WebResource anonymousResource;
    private NewsSubscriber adminSubscription;
    private NewsSubscriber bobSubscription;
    private User joeUser;

    NewsSubscriber createNewsSubscription(User user) {
        EntityManager em = emf.createEntityManager();
        em.getTransaction().begin();
        NewsSubscriber ns = new NewsSubscriber();
        ns.setEmail(user.getEmail());
        ns.setFirstName(user.getFirstName());
        ns.setLastName(user.getLastName());
        em.persist(ns);
        em.getTransaction().commit();
        return ns;
    }

    @Before
    public void setUp() throws Exception {
        setupUpTest();
        User adminUser = createUser("admin@foo.com", "admin", "Mr", "Admin", Role.ADMIN);
        adminResource = createResource("http://localhost/news_list", adminUser, "admin");

        User bobUser = createUser("bob@foo.com", "bob", "Mr", "Bob", Role.USER);
        bobResource = createResource("http://localhost/news_list", bobUser, "bob");

        joeUser = createUser("joe@foo.com", "joe", "Mr", "Joe", Role.USER);
        joeResource = createResource("http://localhost/news_list", joeUser, "joe");

        anonymousResource = createAnonymousResource("http://localhost/news_list");

        adminSubscription = createNewsSubscription(adminUser);
        bobSubscription = createNewsSubscription(bobUser);
    }

    void unsubscribe(NewsSubscriber subscriber) throws Exception {
        anonymousResource.path(subscriber.getEmail()).path(subscriber.getUnsubscribeKey()).delete();
    }

    @Test
    public void testGetSubscribers() throws Exception {
        List<Protocol.NewsSubscriber> lst = adminResource.get(NewsSubscriberList.class).getSubscribersList();
        assertThat(lst.size(), is(2));

        HashSet<String> emails = new HashSet<String>();
        emails.add(lst.get(0).getEmail());
        emails.add(lst.get(1).getEmail());

        assertThat(emails, is(new HashSet<String>(Arrays.asList(adminSubscription.getEmail(), bobSubscription.getEmail()))));
    }

    @Test
    public void testGetSubscribersForbidden() throws Exception {
        ClientResponse resp = bobResource.get(ClientResponse.class);
        assertThat(resp.getClientResponseStatus(), is(ClientResponse.Status.FORBIDDEN));
    }

    @Test
    public void testSubscribe() throws Exception {
        NewsSubscriberList lst = adminResource.get(NewsSubscriberList.class);
        assertThat(lst.getSubscribersList().size(), is(2));

        joeResource.path(joeUser.getId().toString()).path("subscribe").put();
        lst = adminResource.get(NewsSubscriberList.class);
        assertThat(lst.getSubscribersList().size(), is(3));

        joeResource.path(joeUser.getId().toString()).path("subscribe").delete();
        lst = adminResource.get(NewsSubscriberList.class);
        assertThat(lst.getSubscribersList().size(), is(2));

        joeResource.path(joeUser.getId().toString()).path("subscribe").put();
        lst = adminResource.get(NewsSubscriberList.class);
        assertThat(lst.getSubscribersList().size(), is(3));
    }

    @Test
    public void testUnsubscribeWithKey() throws Exception {
        NewsSubscriberList lst = adminResource.get(NewsSubscriberList.class);
        assertThat(lst.getSubscribersList().size(), is(2));

        unsubscribe(adminSubscription);
        unsubscribe(bobSubscription);

        lst = adminResource.get(NewsSubscriberList.class);
        assertThat(lst.getSubscribersList().size(), is(0));
    }

    @Test
    public void testUnsubscribeWithKeyWrongEmailKey() throws Exception {
        NewsSubscriberList lst = adminResource.get(NewsSubscriberList.class);
        assertThat(lst.getSubscribersList().size(), is(2));

        ClientResponse resp = anonymousResource.path(bobSubscription.getEmail()).path(bobSubscription.getUnsubscribeKey() + "xxx").delete(ClientResponse.class);
        assertThat(resp.getClientResponseStatus(), is(ClientResponse.Status.NOT_FOUND));

        resp = anonymousResource.path(bobSubscription.getEmail() + "xxx").path(bobSubscription.getUnsubscribeKey()).delete(ClientResponse.class);
        assertThat(resp.getClientResponseStatus(), is(ClientResponse.Status.NOT_FOUND));

        lst = adminResource.get(NewsSubscriberList.class);
        assertThat(lst.getSubscribersList().size(), is(2));
    }

}
