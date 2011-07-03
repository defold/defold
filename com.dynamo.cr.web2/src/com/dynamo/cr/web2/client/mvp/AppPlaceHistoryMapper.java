package com.dynamo.cr.web2.client.mvp;

import com.dynamo.cr.web2.client.place.DashboardPlace;
import com.dynamo.cr.web2.client.place.GoodbyePlace;
import com.dynamo.cr.web2.client.place.HelloPlace;
import com.dynamo.cr.web2.client.place.LoginPlace;
import com.dynamo.cr.web2.client.place.ProductInfoPlace;
import com.dynamo.cr.web2.client.place.ProjectPlace;
import com.google.gwt.place.shared.PlaceHistoryMapper;
import com.google.gwt.place.shared.WithTokenizers;

/**
 * PlaceHistoryMapper interface is used to attach all places which the
 * PlaceHistoryHandler should be aware of. This is done via the @WithTokenizers
 * annotation or by extending PlaceHistoryMapperWithFactory and creating a
 * separate TokenizerFactory.
 */
@WithTokenizers({ HelloPlace.Tokenizer.class, GoodbyePlace.Tokenizer.class,
        LoginPlace.Tokenizer.class, ProductInfoPlace.Tokenizer.class, DashboardPlace.Tokenizer.class, ProjectPlace.Tokenizer.class })
public interface AppPlaceHistoryMapper extends PlaceHistoryMapper {
}
