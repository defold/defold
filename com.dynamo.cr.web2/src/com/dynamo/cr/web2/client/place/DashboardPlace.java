package com.dynamo.cr.web2.client.place;

import com.google.gwt.place.shared.Place;
import com.google.gwt.place.shared.PlaceTokenizer;

public class DashboardPlace extends Place {
    public static class Tokenizer implements PlaceTokenizer<DashboardPlace> {
        @Override
        public String getToken(DashboardPlace place) {
            return "dashboard";
        }

        @Override
        public DashboardPlace getPlace(String token) {
            return new DashboardPlace();
        }
    }
}
