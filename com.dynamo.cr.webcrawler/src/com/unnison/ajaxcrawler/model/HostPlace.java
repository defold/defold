package com.unnison.ajaxcrawler.model;

import com.google.appengine.api.datastore.Key;

import org.slim3.datastore.Attribute;
import org.slim3.datastore.Model;

import java.util.Date;

@Model
public class HostPlace {

    @Attribute(primaryKey = true)
    private Key key;
    
    @Attribute(lob=true)
    private String html;
    
    private Date lastFetch;

    public void setKey(Key key) {
        this.key = key;
    }

    public Key getKey() {
        return key;
    }

    public void setHtml(String html) {
        this.html = html;
    }

    public String getHtml() {
        return html;
    }

    public void setLastFetch(Date lastFetch) {
        this.lastFetch = lastFetch;
    }

    public Date getLastFetch() {
        return lastFetch;
    }
}
