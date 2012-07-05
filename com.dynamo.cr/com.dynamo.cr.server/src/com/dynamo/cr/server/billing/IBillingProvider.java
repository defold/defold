package com.dynamo.cr.server.billing;

import javax.ws.rs.WebApplicationException;

import com.dynamo.cr.server.model.UserSubscription;

public interface IBillingProvider {
    public void reactivateSubscription(UserSubscription subscription) throws WebApplicationException;

    public void migrateSubscription(UserSubscription subscription, int newProductId) throws WebApplicationException;

    public void cancelSubscription(UserSubscription subscription) throws WebApplicationException;

    public UserSubscription getSubscription(Long subscriptionId) throws WebApplicationException;
}
