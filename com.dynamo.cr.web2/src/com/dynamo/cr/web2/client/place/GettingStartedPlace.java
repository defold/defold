package com.dynamo.cr.web2.client.place;

import com.google.gwt.place.shared.Place;
import com.google.gwt.place.shared.PlaceTokenizer;
import com.google.gwt.place.shared.Prefix;

public class GettingStartedPlace extends Place {

    public GettingStartedPlace() {
    }

    @Prefix("getting_started")
    public static class Tokenizer implements PlaceTokenizer<GettingStartedPlace> {
        @Override
        public String getToken(GettingStartedPlace place) {
            return "";
        }

        @Override
        public GettingStartedPlace getPlace(String token) {
            return new GettingStartedPlace();
        }
    }
}
