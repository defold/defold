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
        // Currently not in use. We used to convert hash-bang to hash-links here.
        return mapper.getPlace(token);
    }

    @Override
    public String getToken(Place place) {
        return mapper.getToken(place);
    }

}
