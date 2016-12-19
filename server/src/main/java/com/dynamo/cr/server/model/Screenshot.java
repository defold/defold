package com.dynamo.cr.server.model;

import javax.persistence.*;

@Entity
@Table(name="project_site_screenshoots")
public class Screenshot {

    public enum MediaType {IMAGE, YOUTUBE, VIMEO}

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(nullable = false)
    private String url;

    @Column(nullable = false)
    @Enumerated(EnumType.STRING)
    private MediaType mediaType;

    public Screenshot() {
    }

    public Screenshot(String url, MediaType mediaType) {
        this.url = url;
        this.mediaType = mediaType;
    }

    public Long getId() {
        return id;
    }

    public String getUrl() {
        return url;
    }

    public MediaType getMediaType() {
        return mediaType;
    }
}
