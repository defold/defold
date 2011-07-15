package com.dynamo.cr.web2.client.place;

import com.google.gwt.place.shared.Place;
import com.google.gwt.place.shared.PlaceTokenizer;
import com.google.gwt.place.shared.Prefix;

public class OpenIDPlace extends Place {
    private String loginToken;
    private String mode;

    public OpenIDPlace(String loginToken, String mode) {
        this.loginToken = loginToken;
        this.mode = mode;
    }

    public String getLoginToken() {
        return loginToken;
    }

    public String getMode() {
        return mode;
    }

    @Prefix("openid")
    public static class Tokenizer implements PlaceTokenizer<OpenIDPlace> {
        @Override
        public String getToken(OpenIDPlace place) {
            return place.getLoginToken();
        }

        @Override
        public OpenIDPlace getPlace(String token) {
            int i = token.indexOf('_');
            return new OpenIDPlace(token.substring(0, i), token.substring(i+1));
        }
    }
}
