package com.dynamo.cr.server.billing;

import com.dynamo.cr.server.model.UserSubscription;

public interface IBillingProvider {
    public boolean reactivateSubscription(UserSubscription subscription);

    public boolean migrateSubscription(UserSubscription subscription, int newProductId);

    public boolean cancelSubscription(UserSubscription subscription);

    public UserSubscription getSubscription(Long subscriptionId);
}
