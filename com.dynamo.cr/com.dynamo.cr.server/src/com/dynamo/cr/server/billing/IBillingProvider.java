package com.dynamo.cr.server.billing;

import javax.ws.rs.WebApplicationException;

import com.dynamo.cr.proto.Config.BillingProduct;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.model.UserSubscription;

public interface IBillingProvider {
    public UserSubscription signUpUser(BillingProduct product, User user, String creditCard, String ccExpirationMonth,
            String ccExpirationYear, String cvv) throws WebApplicationException;

    public void reactivateSubscription(UserSubscription subscription) throws WebApplicationException;

    public void migrateSubscription(UserSubscription subscription, BillingProduct product)
            throws WebApplicationException;

    public void cancelSubscription(UserSubscription subscription) throws WebApplicationException;

    public UserSubscription getSubscription(Long subscriptionId) throws WebApplicationException;
}
