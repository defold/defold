package com.dynamo.cr.web2.client;

import com.google.gwt.core.client.JsArray;

public class ProjectInfo extends BaseResponse {
    protected ProjectInfo() {
    }

    // TODO: This should be a long but not representable in javascript
    public final native int getId() /*-{
        return this.id;
    }-*/;

    public final native String getName() /*-{
        return this.name;
    }-*/;

    public final native void setName(String name) /*-{
        this.name = name;
    }-*/;

    public final native String getDescription() /*-{
        return this.description;
    }-*/;

    public final native void setDescription(String description) /*-{
        this.description = description;
    }-*/;

    public final native UserInfo getOwner() /*-{
        return this.owner;
    }-*/;

    public final native JsArray<UserInfo> getMembers() /*-{
        return this.members;
    }-*/;

    // TODO: This should be a long but not representable in javascript
    public final native int getCreated() /*-{
        return this.created;
    }-*/;

    // TODO: This should be a long but not representable in javascript
    public final native int getLastUpdated() /*-{
        return this.lastUpdated;
    }-*/;


    public final native String getiOSExecutableUrl() /*-{
        return this.iOSExecutableUrl;
    }-*/;

}
