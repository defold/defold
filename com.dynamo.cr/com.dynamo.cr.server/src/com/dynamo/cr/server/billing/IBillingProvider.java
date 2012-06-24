package com.dynamo.cr.server.billing;

import com.dynamo.cr.server.model.Product;
import com.dynamo.cr.server.model.UserSubscription;

public interface IBillingProvider {
    public boolean reactivateSubscription(UserSubscription subscription);

    public boolean migrateSubscription(UserSubscription subscription, Product newProduct);

    public boolean cancelSubscription(UserSubscription subscription);
}
