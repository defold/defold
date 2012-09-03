package com.dynamo.cr.web2.client.place;

import com.google.gwt.place.shared.PlaceTokenizer;
import com.google.gwt.place.shared.Prefix;

public class SettingsPlace extends DefoldPlace {

    @Override
    public String getTitle() {
        return "Settings";
    }

    @Prefix("settings")
    public static class Tokenizer implements PlaceTokenizer<SettingsPlace> {
        @Override
        public String getToken(SettingsPlace place) {
            return "";
        }

        @Override
        public SettingsPlace getPlace(String token) {
            return new SettingsPlace();
        }
    }
}
