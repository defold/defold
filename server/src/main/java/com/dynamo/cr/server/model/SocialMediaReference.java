package com.dynamo.cr.server.model;

import javax.persistence.*;

@Entity
@Table(name="social_media_references")
public class SocialMediaReference {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column
    private String label;

    @Column
    private String url;

    public SocialMediaReference() {
    }

    public SocialMediaReference(String label, String url) {
        this.label = label;
        this.url = url;
    }

    public Long getId() {
        return id;
    }

    public String getLabel() {
        return label;
    }

    public String getUrl() {
        return url;
    }
}
