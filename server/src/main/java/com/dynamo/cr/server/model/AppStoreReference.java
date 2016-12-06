package com.dynamo.cr.server.model;

import javax.persistence.*;

@Entity
@Table(name="app_store_references")
public class AppStoreReference {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column
    private String label;

    @Column
    private String url;

    public AppStoreReference() {
    }

    public AppStoreReference(String label, String url) {
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

    public void setLabel(String label) {
        this.label = label;
    }

    public void setUrl(String url) {
        this.url = url;
    }
}
