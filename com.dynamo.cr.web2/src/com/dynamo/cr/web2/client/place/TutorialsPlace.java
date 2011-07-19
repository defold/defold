package com.dynamo.cr.web2.client.place;

import com.google.gwt.place.shared.Place;
import com.google.gwt.place.shared.PlaceTokenizer;
import com.google.gwt.place.shared.Prefix;

public class TutorialsPlace extends Place {
    @Prefix("tutorials")
    public static class Tokenizer implements PlaceTokenizer<TutorialsPlace> {
        @Override
        public String getToken(TutorialsPlace place) {
            return "";
        }

        @Override
        public TutorialsPlace getPlace(String token) {
            return new TutorialsPlace();
        }
    }
}
