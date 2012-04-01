package com.dynamo.cr.web2.client;

import com.google.gwt.place.shared.Place;
import com.google.gwt.place.shared.PlaceHistoryMapper;

public class FilterPlaceHistoryMapper implements PlaceHistoryMapper {

    private PlaceHistoryMapper mapper;

    public FilterPlaceHistoryMapper(PlaceHistoryMapper mapper) {
        this.mapper = mapper;
    }

    @Override
    public Place getPlace(String token) {
        // Remove ! from token. Google ajax-style url:s
        // have are of the hash-bang-form, ie #!.
        // By removing ! we are comptabible with google ajax links
        // used for crawling.
        if (token.startsWith("!")) {
            token = token.substring(1);
        }
        return mapper.getPlace(token);
    }

    @Override
    public String getToken(Place place) {
        return mapper.getToken(place);
    }

}
