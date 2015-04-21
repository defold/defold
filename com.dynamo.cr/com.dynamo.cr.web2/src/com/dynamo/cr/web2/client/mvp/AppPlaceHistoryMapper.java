package com.dynamo.cr.web2.client.mvp;

import com.dynamo.cr.web2.client.place.DashboardPlace;
import com.dynamo.cr.web2.client.place.DownloadsPlace;
import com.dynamo.cr.web2.client.place.InvitePlace;
import com.dynamo.cr.web2.client.place.LoginPlace;
import com.dynamo.cr.web2.client.place.NewProjectPlace;
import com.dynamo.cr.web2.client.place.OAuthPlace;
import com.dynamo.cr.web2.client.place.ProjectPlace;
import com.dynamo.cr.web2.client.place.SettingsPlace;
import com.google.gwt.place.shared.PlaceHistoryMapper;
import com.google.gwt.place.shared.WithTokenizers;

/**
 * PlaceHistoryMapper interface is used to attach all places which the
 * PlaceHistoryHandler should be aware of. This is done via the @WithTokenizers
 * annotation or by extending PlaceHistoryMapperWithFactory and creating a
 * separate TokenizerFactory.
 */
@WithTokenizers({ LoginPlace.Tokenizer.class, DashboardPlace.Tokenizer.class,
        ProjectPlace.Tokenizer.class, NewProjectPlace.Tokenizer.class,
        OAuthPlace.Tokenizer.class, InvitePlace.Tokenizer.class, DownloadsPlace.Tokenizer.class,
        SettingsPlace.Tokenizer.class})
public interface AppPlaceHistoryMapper extends PlaceHistoryMapper {
}
