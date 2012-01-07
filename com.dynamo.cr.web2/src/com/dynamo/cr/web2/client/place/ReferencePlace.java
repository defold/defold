package com.dynamo.cr.web2.client.place;

import com.google.gwt.place.shared.Place;
import com.google.gwt.place.shared.PlaceTokenizer;
import com.google.gwt.place.shared.Prefix;

public class ReferencePlace extends Place {

    private String id;
    public ReferencePlace(String id) {
        this.id = id;
    }

    public String getId() {
        return id;
    }

    @Prefix("reference")
    public static class Tokenizer implements PlaceTokenizer<ReferencePlace> {
        @Override
        public String getToken(ReferencePlace place) {
            return place.getId();
        }

        @Override
        public ReferencePlace getPlace(String token) {
            return new ReferencePlace(token);
        }
    }
}
