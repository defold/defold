package com.dynamo.cr.web2.client.place;

import com.google.gwt.place.shared.PlaceTokenizer;
import com.google.gwt.place.shared.Prefix;

public class SubscriptionPlace extends DefoldPlace {

    @Override
    public String getTitle() {
        return "Plan";
    }

    @Prefix("!plan")
    public static class Tokenizer implements PlaceTokenizer<SubscriptionPlace> {
        @Override
        public String getToken(SubscriptionPlace place) {
            return "";
        }

        @Override
        public SubscriptionPlace getPlace(String token) {
            return new SubscriptionPlace();
        }
    }
}
