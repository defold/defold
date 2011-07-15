package com.dynamo.cr.web2.client.place;

import com.google.gwt.place.shared.Place;
import com.google.gwt.place.shared.PlaceTokenizer;
import com.google.gwt.place.shared.Prefix;

public class NewProjectPlace extends Place {

    @Prefix("new_project")
    public static class Tokenizer implements PlaceTokenizer<NewProjectPlace> {
        @Override
        public String getToken(NewProjectPlace place) {
            return "";
        }

        @Override
        public NewProjectPlace getPlace(String token) {
            return new NewProjectPlace();
        }
    }
}
