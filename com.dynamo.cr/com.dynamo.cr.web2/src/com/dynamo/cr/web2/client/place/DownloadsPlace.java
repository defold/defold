package com.dynamo.cr.web2.client.place;

import com.google.gwt.place.shared.PlaceTokenizer;
import com.google.gwt.place.shared.Prefix;

public class DownloadsPlace extends DefoldPlace {

    @Override
    public String getTitle() {
        return "Downloads";
    }

    @Prefix("!downloads")
    public static class Tokenizer implements PlaceTokenizer<DownloadsPlace> {
        @Override
        public String getToken(DownloadsPlace place) {
            return "";
        }

        @Override
        public DownloadsPlace getPlace(String token) {
            return new DownloadsPlace();
        }
    }
}
