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
        // Twitter, gmail, etc remove the trailing colon in links
        // If no colon exists in url we append one.
        if (token.indexOf(':') == -1) {
            token += ":";
        }
        return mapper.getPlace(token);
    }

    @Override
    public String getToken(Place place) {
        return mapper.getToken(place);
    }

}
