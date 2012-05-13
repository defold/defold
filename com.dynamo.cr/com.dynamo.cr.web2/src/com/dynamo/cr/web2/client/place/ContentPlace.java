package com.dynamo.cr.web2.client.place;

import com.google.gwt.place.shared.PlaceTokenizer;
import com.google.gwt.place.shared.Prefix;

public class ContentPlace extends DefoldPlace {

    private String id;

    public ContentPlace(String id) {
        this.id = id;
    }

    public String getId() {
        return id;
    }

    @Override
    public String getTitle() {
        return niceify(id);
    }

    @Prefix("!content")
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
