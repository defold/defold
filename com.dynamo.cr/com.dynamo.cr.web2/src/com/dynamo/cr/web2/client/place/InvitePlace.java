package com.dynamo.cr.web2.client.place;

import com.google.gwt.place.shared.PlaceTokenizer;
import com.google.gwt.place.shared.Prefix;

public class InvitePlace extends DefoldPlace {

    @Override
    public String getTitle() {
        return "Invite";
    }

    @Prefix("invite")
    public static class Tokenizer implements PlaceTokenizer<InvitePlace> {
        @Override
        public String getToken(InvitePlace place) {
            return "";
        }

        @Override
        public InvitePlace getPlace(String token) {
            return new InvitePlace();
        }
    }
}
