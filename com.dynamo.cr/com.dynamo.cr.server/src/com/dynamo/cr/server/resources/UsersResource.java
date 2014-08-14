package com.dynamo.cr.server.resources;

import java.util.Set;

import javax.annotation.security.RolesAllowed;
import javax.ws.rs.DELETE;
import javax.ws.rs.GET;
import javax.ws.rs.POST;
import javax.ws.rs.PUT;
import javax.ws.rs.Path;
import javax.ws.rs.PathParam;
import javax.ws.rs.QueryParam;
import javax.ws.rs.WebApplicationException;
import javax.ws.rs.core.Response;
import javax.ws.rs.core.Response.Status;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.slf4j.Marker;
import org.slf4j.MarkerFactory;

import com.dynamo.cr.proto.Config.BillingProduct;
import com.dynamo.cr.protocol.proto.Protocol.InvitationAccountInfo;
import com.dynamo.cr.protocol.proto.Protocol.RegisterUser;
import com.dynamo.cr.protocol.proto.Protocol.UserInfo;
import com.dynamo.cr.protocol.proto.Protocol.UserInfoList;
import com.dynamo.cr.protocol.proto.Protocol.UserSubscriptionInfo;
import com.dynamo.cr.protocol.proto.Protocol.UserSubscriptionState;
import com.dynamo.cr.server.ServerException;
import com.dynamo.cr.server.billing.IBillingProvider;
import com.dynamo.cr.server.model.InvitationAccount;
import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.model.UserSubscription;
import com.dynamo.cr.server.model.UserSubscription.State;
import com.dynamo.inject.persist.Transactional;

@Path("/users")
@RolesAllowed(value = { "user" })
public class UsersResource extends BaseResource {

    private static Logger logger = LoggerFactory.getLogger(UsersResource.class);
    private static final Marker BILLING_MARKER = MarkerFactory.getMarker("BILLING");

    static UserInfo createUserInfo(User u) {
        UserInfo.Builder b = UserInfo.newBuilder();
        b.setId(u.getId())
         .setEmail(u.getEmail())
         .setFirstName(u.getFirstName())
         .setLastName(u.getLastName());
        return b.build();
    }

    static InvitationAccountInfo createInvitationAccountInfo(InvitationAccount a) {
        InvitationAccountInfo.Builder b = InvitationAccountInfo.newBuilder();
        b.setOriginalCount(a.getOriginalCount())
.setCurrentCount(a.getCurrentCount());
        return b.build();
    }

    @GET
    @Path("/{email}")
    public UserInfo getUserInfo(@PathParam("email") String email) {
        User u = ModelUtil.findUserByEmail(em, email);
        if (u == null) {
            throw new WebApplicationException(Response.Status.NOT_FOUND);
        }

        return createUserInfo(u);
    }

    @PUT
    @Path("/{user}/connections/{user2}")
    @Transactional
    public void connect(@PathParam("user2") String user2) {
        User u = getUser();
        User u2 = server.getUser(em, user2);
        if (u.getId() == u2.getId()) {
            throw new ServerException("A user can not be connected to him/herself.", Response.Status.FORBIDDEN);
        }

        u.getConnections().add(u2);
        em.persist(u);
    }

    @GET
    @Path("/{user}/connections")
    public UserInfoList getConnections() {
        User u = getUser();

        Set<User> connections = u.getConnections();
        UserInfoList.Builder b = UserInfoList.newBuilder();
        for (User connection : connections) {
            b.addUsers(createUserInfo(connection));
        }

        return b.build();
    }

    @POST
    @RolesAllowed(value = { "admin" })
    @Transactional
    public UserInfo registerUser(RegisterUser registerUser) {
        /*
         * NOTE: Old registration method as admin role
         * OpenID registration is the only supported method. We should
         * probably remove this and related tests soon
         */

        User user = new User();
        user.setEmail(registerUser.getEmail());
        user.setFirstName(registerUser.getFirstName());
        user.setLastName(registerUser.getLastName());
        user.setPassword(registerUser.getPassword());
        em.persist(user);
        em.flush();

        UserInfo userInfo = createUserInfo(user);
        return userInfo;
    }

    @PUT
    @Path("/{user}/invite/{email}")
    @Transactional
    public Response invite(@PathParam("user") String user, @PathParam("email") String email) {
        InvitationAccount a = server.getInvitationAccount(em, user);
        if (a.getCurrentCount() == 0) {
            throwWebApplicationException(Status.FORBIDDEN, "Inviter has no invitations left");
        }
        a.setCurrentCount(a.getCurrentCount() - 1);
        em.persist(a);

        User u = getUser();
        String inviter = String.format("%s %s", u.getFirstName(), u.getLastName());
        server.invite(em, email, inviter, u.getEmail(), a.getOriginalCount());

        return okResponse("User %s invited", email);
    }

    @GET
    @Path("/{user}/invitation_account")
    @Transactional
    public InvitationAccountInfo getInvitationAccount(@PathParam("user") String user) {
        InvitationAccount a = server.getInvitationAccount(em, user);
        return createInvitationAccountInfo(a);
    }

    @POST
    @Path("/{user}/subscription")
    @Transactional
    public UserSubscriptionInfo subscribe(@QueryParam("external_id") String externalId) {
        User u = getUser();
        UserSubscription us = ModelUtil.findUserSubscriptionByUser(em, u);
        if (us == null) {
            UserSubscription subscription = server.getBillingProvider().getSubscription(Long.parseLong(externalId));
            subscription.setUser(u);
            em.persist(subscription);
            em.flush();
            logger.info(BILLING_MARKER, String.format("Subscription %d (external %d) created for user %s.",
                    subscription.getId(), subscription.getExternalId(), u.getEmail()));
            return ResourceUtil.createUserSubscriptionInfo(subscription, server.getConfiguration());
        } else {
            logger.error(BILLING_MARKER,
                    String.format(
                            "Could not create subscription (external %d) for user %s since a subscription already exists.",
                            externalId, u.getEmail()));
            throwWebApplicationException(Status.CONFLICT, "User already has subscription");
        }
        return null;
    }

    @GET
    @Path("/{user}/subscription")
    @Transactional
    public UserSubscriptionInfo getSubscription() {
        User u = getUser();
        UserSubscription us = ModelUtil.findUserSubscriptionByUser(em, u);
        if (us != null) {
            return ResourceUtil.createUserSubscriptionInfo(us, server.getConfiguration());
        } else {
            UserSubscriptionInfo.Builder builder = UserSubscriptionInfo.newBuilder();
            BillingProduct defaultProduct = null;
            for (BillingProduct product : server.getConfiguration().getProductsList()) {
                if (product.getDefault() != 0) {
                    defaultProduct = product;
                    break;
                }
            }
            if (defaultProduct == null) {
                throwWebApplicationException(Status.INTERNAL_SERVER_ERROR,
                        "Could not find a default product");
            }
            builder.setState(UserSubscriptionState.ACTIVE);
            builder.setProduct(ResourceUtil.createProductInfo(defaultProduct, server.getConfiguration()
                    .getBillingApiUrl()));
            return builder.build();
        }
    }

    @PUT
    @Path("/{user}/subscription")
    @Transactional
    public UserSubscriptionInfo setSubscription(
            @QueryParam("product") String product, @QueryParam("state") String state,
            @QueryParam("external_id") String externalId) {
        User u = getUser();
        UserSubscription us = ModelUtil.findUserSubscriptionByUser(em, u);
        if (us != null) {
            int nonNull = 0;
            nonNull += product != null ? 1 : 0;
            nonNull += state != null ? 1 : 0;
            nonNull += externalId != null ? 1 : 0;
            if (nonNull > 1) {
                throwWebApplicationException(Status.CONFLICT,
                        "Only one of the parameters product, state and external_id is accepted at a time.");
            }
            IBillingProvider billingProvider = server.getBillingProvider();
            // Migrate subscription
            if (product != null) {
                BillingProduct newProduct = server.getProduct(product);
                if (newProduct.getId() != us.getProductId()) {
                    if (us.getState() != State.ACTIVE) {
                        throwWebApplicationException(Status.CONFLICT, "Only active subscriptions can be updated");
                    }
                    billingProvider.migrateSubscription(us, newProduct);
                    us.setProductId((long) newProduct.getId());
                }
            }
            // Update state
            if (state != null) {
                State newState = State.valueOf(state);
                State oldState = us.getState();
                if (oldState != newState) {
                    if (newState != State.ACTIVE) {
                        throwWebApplicationException(Status.CONFLICT, "Subscriptions can only be manually activated");
                    }
                    if (oldState == State.CANCELED) {
                        billingProvider.reactivateSubscription(us);
                    }
                }
            }
            // Update from provider
            if (externalId != null) {
                UserSubscription s = server.getBillingProvider().getSubscription(Long.parseLong(externalId));
                us.setCreditCard(s.getCreditCard());
                us.setExternalId(s.getExternalId());
                us.setExternalCustomerId(s.getExternalCustomerId());
                us.setProductId(s.getProductId());
                us.setState(s.getState());
                logger.info(
                        BILLING_MARKER,
                        String.format("User %s changed payment details for subscription %d (external %d).",
                                u.getEmail(), us.getId(), us.getExternalId()));
            }
            em.persist(us);
            em.flush();
            return ResourceUtil.createUserSubscriptionInfo(us, server.getConfiguration());
        } else {
            throwWebApplicationException(Status.NOT_FOUND, "User has no subscription");
        }
        return null;
    }

    @DELETE
    @Path("/{user}/subscription")
    @Transactional
    public void deleteSubscription() {
        User u = getUser();
        UserSubscription us = ModelUtil.findUserSubscriptionByUser(em, u);
        if (us != null) {
            if (us.getState() == State.CANCELED) {
                Long subscriptionId = us.getId();
                Long externalId = us.getExternalId();
                em.remove(us);
                logger.info(BILLING_MARKER, "Subscription {} (external {}) for user {} was permanently terminated.",
                        new Object[] { subscriptionId, externalId, u.getEmail() });
            } else {
                logger.error(BILLING_MARKER,
                        "Forbidden attempt to delete subscription {} (external {}), which is not yet canceled.",
                        us.getId(), us.getExternalCustomerId());
                throwWebApplicationException(Status.CONFLICT, "User has no subscription");
            }
        } else {
            logger.error(BILLING_MARKER,
                    "Could not permanently terminate subscription for user {} since it's missing.", u.getEmail());
            throwWebApplicationException(Status.NOT_FOUND, "User has no subscription");
        }
    }

}

