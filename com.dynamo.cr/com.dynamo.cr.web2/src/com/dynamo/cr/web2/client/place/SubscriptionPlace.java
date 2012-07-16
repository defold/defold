package com.dynamo.cr.web2.client.place;

import com.google.gwt.place.shared.PlaceTokenizer;
import com.google.gwt.place.shared.Prefix;

public class SubscriptionPlace extends DefoldPlace {
    private String subscriptionId;

    public SubscriptionPlace() {
    }

    public SubscriptionPlace(String subscriptionId) {
        this.subscriptionId = subscriptionId;
    }

    @Override
    public String getTitle() {
        return "Plan";
    }

    public String getSubscriptionId() {
        return this.subscriptionId;
    }

    @Prefix("plan")
    public static class Tokenizer implements PlaceTokenizer<SubscriptionPlace> {
        @Override
        public String getToken(SubscriptionPlace place) {
            String subscriptionId = place.getSubscriptionId();
            if (subscriptionId != null) {
                return subscriptionId;
            }
            return "";
        }

        @Override
        public SubscriptionPlace getPlace(String token) {
            if (token != null && !token.isEmpty()) {
                return new SubscriptionPlace(token);
            } else {
                return new SubscriptionPlace();
            }
        }
    }
}
