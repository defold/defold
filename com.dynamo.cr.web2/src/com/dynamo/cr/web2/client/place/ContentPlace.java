package com.dynamo.cr.web2.client.place;

import com.google.gwt.place.shared.Place;
import com.google.gwt.place.shared.PlaceTokenizer;
import com.google.gwt.place.shared.Prefix;

public class ContentPlace extends Place {

    private String id;

    public ContentPlace(String id) {
        this.id = id;
    }

    public String getId() {
        return id;
    }

    @Prefix("content")
    public static class Tokenizer implements PlaceTokenizer<ContentPlace> {
        @Override
        public String getToken(ContentPlace place) {
            return place.getId();
        }

        @Override
        public ContentPlace getPlace(String token) {
            return new ContentPlace(token);
        }
    }
}
