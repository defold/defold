package com.dynamo.cr.web2.client.place;

import com.google.gwt.place.shared.Place;
import com.google.gwt.place.shared.PlaceTokenizer;
import com.google.gwt.place.shared.Prefix;

public class DocumentationPlace extends Place {
    @Prefix("documentation")
    public static class Tokenizer implements PlaceTokenizer<DocumentationPlace> {
        @Override
        public String getToken(DocumentationPlace place) {
            return "";
        }

        @Override
        public DocumentationPlace getPlace(String token) {
            return new DocumentationPlace();
        }
    }
}
