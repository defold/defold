package com.dynamo.cr.web2.client.place;

import com.google.gwt.place.shared.Place;
import com.google.gwt.place.shared.PlaceTokenizer;
import com.google.gwt.place.shared.Prefix;

public class ScriptSamplePlace extends Place {

    public ScriptSamplePlace() {
    }

    @Prefix("script_sample")
    public static class Tokenizer implements PlaceTokenizer<ScriptSamplePlace> {
        @Override
        public String getToken(ScriptSamplePlace place) {
            return "";
        }

        @Override
        public ScriptSamplePlace getPlace(String token) {
            return new ScriptSamplePlace();
        }
    }
}
