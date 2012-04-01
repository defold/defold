package com.unnison.ajaxcrawler.model;

import com.google.appengine.api.datastore.Key;

import org.slim3.datastore.Attribute;
import org.slim3.datastore.Model;

import java.util.ArrayList;
import java.util.Date;

@Model
public class Host {

    @Attribute(primaryKey = true)
    private Key key;
    
    private ArrayList<String> indexes;
    
    private Date lastFetch;
    
    private int fetchRatio;
    
    public void setKey(Key key) {
        this.key = key;
    }

    public Key getKey() {
        return key;
    }

    public void setFetchRatio(int fetchRatio) {
        this.fetchRatio = fetchRatio;
    }

    public int getFetchRatio() {
        return fetchRatio;
    }

    public void setIndexes(ArrayList<String> indexes) {
        this.indexes = indexes;
    }

    public ArrayList<String> getIndexes() {
        return indexes;
    }

    public void setLastFetch(Date lastFetch) {
        this.lastFetch = lastFetch;
    }

    public Date getLastFetch() {
        return lastFetch;
    }
}
