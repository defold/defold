package com.dynamo.cr.web2.client.mvp;

import com.dynamo.cr.web2.client.place.BlogPlace;
import com.dynamo.cr.web2.client.place.DashboardPlace;
import com.dynamo.cr.web2.client.place.DocumentationPlace;
import com.dynamo.cr.web2.client.place.GuidePlace;
import com.dynamo.cr.web2.client.place.LoginPlace;
import com.dynamo.cr.web2.client.place.NewProjectPlace;
import com.dynamo.cr.web2.client.place.OpenIDPlace;
import com.dynamo.cr.web2.client.place.ProductInfoPlace;
import com.dynamo.cr.web2.client.place.ProjectPlace;
import com.dynamo.cr.web2.client.place.ReferencePlace;
import com.dynamo.cr.web2.client.place.ScriptSamplePlace;
import com.dynamo.cr.web2.client.place.TutorialsPlace;
import com.google.gwt.place.shared.PlaceHistoryMapper;
import com.google.gwt.place.shared.WithTokenizers;

/**
 * PlaceHistoryMapper interface is used to attach all places which the PlaceHistoryHandler should be
 * aware of. This is done via the @WithTokenizers annotation or by extending
 * PlaceHistoryMapperWithFactory and creating a separate TokenizerFactory.
 */
@WithTokenizers({ LoginPlace.Tokenizer.class, ProductInfoPlace.Tokenizer.class,
        DashboardPlace.Tokenizer.class, ProjectPlace.Tokenizer.class,
        NewProjectPlace.Tokenizer.class, OpenIDPlace.Tokenizer.class,
        DocumentationPlace.Tokenizer.class, TutorialsPlace.Tokenizer.class,
        BlogPlace.Tokenizer.class, ScriptSamplePlace.Tokenizer.class,
        ReferencePlace.Tokenizer.class, GuidePlace.Tokenizer.class })
public interface AppPlaceHistoryMapper extends PlaceHistoryMapper {
}
