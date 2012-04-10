package com.dynamo.cr.web2.client.place;

import com.google.gwt.place.shared.PlaceTokenizer;
import com.google.gwt.place.shared.Prefix;

public class TutorialsPlace extends DefoldPlace {

    private String id;
    public TutorialsPlace(String id) {
        this.id = id;
    }

    public String getId() {
        return id;
    }

    @Override
    public String getTitle() {
        return niceify(id);
    }

    @Prefix("tutorials")
    public static class Tokenizer implements PlaceTokenizer<TutorialsPlace> {
        @Override
        public String getToken(TutorialsPlace place) {
            return place.getId();
        }

        @Override
        public TutorialsPlace getPlace(String token) {
            return new TutorialsPlace(token);
        }
    }
}
