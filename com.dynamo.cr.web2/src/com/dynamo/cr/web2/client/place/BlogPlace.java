package com.dynamo.cr.web2.client.place;

import com.google.gwt.place.shared.Place;
import com.google.gwt.place.shared.PlaceTokenizer;
import com.google.gwt.place.shared.Prefix;

public class BlogPlace extends Place {

    public BlogPlace() {
    }

    @Prefix("blog")
    public static class Tokenizer implements PlaceTokenizer<BlogPlace> {
        @Override
        public String getToken(BlogPlace place) {
            return "";
        }

        @Override
        public BlogPlace getPlace(String token) {
            return new BlogPlace();
        }
    }
}
