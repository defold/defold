package com.dynamo.cr.web2.client.place;

import com.google.gwt.place.shared.Place;
import com.google.gwt.place.shared.PlaceTokenizer;

public class NewProjectPlace extends Place {
    public static class Tokenizer implements PlaceTokenizer<NewProjectPlace> {
        @Override
        public String getToken(NewProjectPlace place) {
            return "new_project";
        }

        @Override
        public NewProjectPlace getPlace(String token) {
            return new NewProjectPlace();
        }
    }
}
