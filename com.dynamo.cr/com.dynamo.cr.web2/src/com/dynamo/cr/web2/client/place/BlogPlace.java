package com.dynamo.cr.web2.client.place;

import com.google.gwt.place.shared.PlaceTokenizer;
import com.google.gwt.place.shared.Prefix;

public class BlogPlace extends DefoldPlace {

    public BlogPlace() {
    }

    @Override
    public String getTitle() {
        return "Blog";
    }

    @Prefix("!blog")
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
