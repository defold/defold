package com.dynamo.cr.web2.client.place;

import com.google.gwt.place.shared.PlaceTokenizer;
import com.google.gwt.place.shared.Prefix;

public class ScriptSamplePlace extends DefoldPlace {

    private String id;
    public ScriptSamplePlace(String id) {
        this.id = id;
    }

    public String getId() {
        return id;
    }

    @Override
    public String getTitle() {
        return niceify(id);
    }

    @Prefix("script_sample")
    public static class Tokenizer implements PlaceTokenizer<ScriptSamplePlace> {
        @Override
        public String getToken(ScriptSamplePlace place) {
            return place.getId();
        }

        @Override
        public ScriptSamplePlace getPlace(String token) {
            return new ScriptSamplePlace(token);
        }
    }
}
