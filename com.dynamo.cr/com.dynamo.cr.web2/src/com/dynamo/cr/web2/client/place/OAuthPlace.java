package com.dynamo.cr.web2.client.place;

import com.google.gwt.place.shared.Place;
import com.google.gwt.place.shared.PlaceTokenizer;
import com.google.gwt.place.shared.Prefix;

public class OAuthPlace extends Place {
    private String loginToken;
    private String mode;

    public OAuthPlace(String loginToken, String mode) {
        this.loginToken = loginToken;
        this.mode = mode;
    }

    public String getLoginToken() {
        return loginToken;
    }

    public String getMode() {
        return mode;
    }

    @Prefix("oauth")
    public static class Tokenizer implements PlaceTokenizer<OAuthPlace> {
        @Override
        public String getToken(OAuthPlace place) {
            return place.getLoginToken();
        }

        @Override
        public OAuthPlace getPlace(String token) {
            int i = token.indexOf('_');
            return new OAuthPlace(token.substring(0, i), token.substring(i+1));
        }
    }
}
