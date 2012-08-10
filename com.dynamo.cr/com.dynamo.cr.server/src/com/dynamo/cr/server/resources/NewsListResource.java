package com.dynamo.cr.server.resources;

import java.util.List;

import javax.annotation.security.RolesAllowed;
import javax.persistence.TypedQuery;
import javax.ws.rs.DELETE;
import javax.ws.rs.GET;
import javax.ws.rs.Path;
import javax.ws.rs.PathParam;
import javax.ws.rs.core.Response;
import javax.ws.rs.core.Response.Status;

import com.dynamo.cr.protocol.proto.Protocol;
import com.dynamo.cr.protocol.proto.Protocol.NewsSubscriberList;
import com.dynamo.cr.server.model.NewsSubscriber;
import com.dynamo.inject.persist.Transactional;

@Path("/news_list")
public class NewsListResource extends BaseResource {

    @DELETE
    @Path("/{email}/{key}")
    @Transactional
    @RolesAllowed(value = {"anonymous"})
    public Response deleteSubscription(@PathParam("email") String email,
                                       @PathParam("key") String key) {

        TypedQuery<NewsSubscriber> q = em.createQuery("select s from NewsSubscriber s where s.email = :email", NewsSubscriber.class);
        List<NewsSubscriber> lst = q.setParameter("email", email).getResultList();
        if (lst.size() == 0) {
            throwWebApplicationException(Status.NOT_FOUND, "Invalid email/key to unsubscribe");
        }

        NewsSubscriber s = lst.get(0);
        if (!s.getUnsubscribeKey().equals(key)) {
            throwWebApplicationException(Status.NOT_FOUND, "Invalid email/key unsubscribe");
        }
        s.setSubscribed(false);
        em.persist(s);
        return okResponse("%s unsubscribed to news-letter", email);
    }

    @GET
    @RolesAllowed(value = { "admin" })
    public NewsSubscriberList getNewsSubscribers() {
        TypedQuery<NewsSubscriber> q = em.createQuery("select s from NewsSubscriber s", NewsSubscriber.class);
        List<NewsSubscriber> lst = q.getResultList();

        NewsSubscriberList.Builder builder = NewsSubscriberList.newBuilder();
        for (NewsSubscriber ns : lst) {
            if (ns.isSubscribed()) {
                Protocol.NewsSubscriber.Builder nsProto = Protocol.NewsSubscriber
                        .newBuilder()
                        .setEmail(ns.getEmail())
                        .setFirstName(ns.getFirstName())
                        .setLastName(ns.getLastName())
                        .setKey(ns.getUnsubscribeKey());

                builder.addSubscribers(nsProto);
            }
        }

        return builder.build();
    }
}
