package com.dynamo.cr.server.resources;

import com.dynamo.cr.protocol.proto.Protocol;
import com.dynamo.cr.protocol.proto.Protocol.NewsSubscriberList;
import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.NewsSubscriber;
import com.dynamo.cr.server.model.User;
import com.dynamo.inject.persist.Transactional;

import javax.annotation.security.RolesAllowed;
import javax.persistence.TypedQuery;
import javax.ws.rs.*;
import javax.ws.rs.core.Response;
import javax.ws.rs.core.Response.Status;
import java.util.List;

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
            throwWebApplicationException(Status.NOT_FOUND, "Invalid email/key when unsubscribing");
        }

        NewsSubscriber s = lst.get(0);
        if (!s.getUnsubscribeKey().equals(key)) {
            throwWebApplicationException(Status.NOT_FOUND, "Invalid email/key when unsubscribing");
        }
        s.setSubscribed(false);
        em.persist(s);
        return okResponse("%s unsubscribed to news-letter", email);
    }

    @PUT
    @RolesAllowed(value = {"user"})
    @Transactional
    @Path("/{user}/subscribe")
    public Response subscribe() {
        User u = getUser();

        TypedQuery<NewsSubscriber> q = em.createQuery("select s from NewsSubscriber s where s.email = :email", NewsSubscriber.class);
        List<NewsSubscriber> lst = q.setParameter("email", u.getEmail()).getResultList();
        if (lst.size() == 0) {
            ModelUtil.subscribeToNewsLetter(em, u.getEmail(), u.getFirstName(), u.getLastName());
            return okResponse("Subscribed");
        } else {
            NewsSubscriber ns = lst.get(0);
            ns.setSubscribed(true);
            em.persist(ns);
            return okResponse("Subscribed");
        }
    }

    @GET
    @RolesAllowed(value = {"user"})
    @Path("/{user}/subscribe")
    public String subscribed() {
        User user = getUser();

        TypedQuery<NewsSubscriber> query = em.createQuery("select s from NewsSubscriber s where s.email = :email", NewsSubscriber.class);
        List<NewsSubscriber> subscribers = query.setParameter("email", user.getEmail()).getResultList();

        if (!subscribers.isEmpty()) {
            NewsSubscriber subscriber = subscribers.get(0);
            if (subscriber.isSubscribed()) {
                return "true";
            }
        }

        return "false";
    }

    @DELETE
    @RolesAllowed(value = {"user"})
    @Transactional
    @Path("/{user}/subscribe")
    public Response unsubscribe() {
        User u = getUser();

        TypedQuery<NewsSubscriber> q = em.createQuery("select s from NewsSubscriber s where s.email = :email", NewsSubscriber.class);
        List<NewsSubscriber> lst = q.setParameter("email", u.getEmail()).getResultList();
        if (lst.size() > 0) {
            NewsSubscriber ns = lst.get(0);
            ns.setSubscribed(false);
            em.persist(ns);
            return okResponse("Unsubscribed");
        } else {
            return okResponse("Unsubscribed");
        }
    }

    @GET
    @RolesAllowed(value = {"admin"})
    public NewsSubscriberList getNewsSubscribers() {
        TypedQuery<NewsSubscriber> query = em.createQuery("select s from NewsSubscriber s", NewsSubscriber.class);
        List<NewsSubscriber> subscribers = query.getResultList();

        NewsSubscriberList.Builder builder = NewsSubscriberList.newBuilder();

        subscribers
                .stream()
                .filter(NewsSubscriber::isSubscribed)
                .forEach(subscriber -> {
                    Protocol.NewsSubscriber.Builder nsProto = Protocol.NewsSubscriber
                            .newBuilder()
                            .setEmail(subscriber.getEmail())
                            .setFirstName(subscriber.getFirstName())
                            .setLastName(subscriber.getLastName())
                            .setKey(subscriber.getUnsubscribeKey());

                    builder.addSubscribers(nsProto);
                });

        return builder.build();
    }
}
