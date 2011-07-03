package com.dynamo.cr.web2.client.place;

import com.google.gwt.place.shared.Place;
import com.google.gwt.place.shared.PlaceTokenizer;

public class ProjectPlace extends Place {

    private String id;
    public ProjectPlace(String id) {
        this.id = id;
    }

    public String getId() {
        return id;
    }

    public static class Tokenizer implements PlaceTokenizer<ProjectPlace> {
        @Override
        public String getToken(ProjectPlace place) {
            return place.getId();
        }

        @Override
        public ProjectPlace getPlace(String token) {
            return new ProjectPlace(token);
        }
    }
}
