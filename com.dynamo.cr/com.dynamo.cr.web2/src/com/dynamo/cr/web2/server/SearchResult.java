package com.dynamo.cr.web2.server;

import java.util.Date;

public class SearchResult {

    private String id;
    private String url;
    private String title;
    private Date published;
    private String content;

    public SearchResult(String id, String url, String title, Date published, String content) {
        this.id = id;
        this.url = url;
        this.title = title;
        this.published = published;
        this.content = content;
    }

    public String getId() {
        return id;
    }

    public String getUrl() {
        return url;
    }

    public String getTitle() {
        return title;
    }

    public Date getPublished() {
        return published;
    }

    public String getContent() {
        return content;
    }
}

