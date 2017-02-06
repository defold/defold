package com.dynamo.cr.server.model;

import javax.persistence.*;
import java.util.HashSet;
import java.util.Set;

@Entity
@Table(name="project_sites")
public class ProjectSite {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column
    private String name;

    @Column(length = 3000)
    private String description;

    @Column
    private String studioUrl;

    @Column
    private String studioName;

    @Column
    private String coverImageUrl;

    @Column
    private String storeFrontImageUrl;

    @Column
    private String devLogUrl;

    @Column
    private String reviewUrl;

    @OneToMany(cascade = CascadeType.ALL)
    private Set<AppStoreReference> appStoreReferences = new HashSet<>();

    @OneToMany(cascade = CascadeType.ALL)
    private Set<Screenshot> screenshots = new HashSet<>();

    public String getName() {
        return name;
    }

    public void setName(String name) {
        this.name = name;
    }

    public String getDescription() {
        return description;
    }

    public void setDescription(String description) {
        this.description = description;
    }

    public String getStudioUrl() {
        return studioUrl;
    }

    public String getStudioName() {
        return studioName;
    }

    public String getCoverImageUrl() {
        return coverImageUrl;
    }

    public String getStoreFrontImageUrl() {
        return storeFrontImageUrl;
    }

    public String getDevLogUrl() {
        return devLogUrl;
    }

    public String getReviewUrl() {
        return reviewUrl;
    }

    public Set<AppStoreReference> getAppStoreReferences() {
        return appStoreReferences;
    }

    public Set<Screenshot> getScreenshots() {
        return screenshots;
    }

    public void setStudioUrl(String studioUrl) {
        this.studioUrl = studioUrl;
    }

    public void setStudioName(String studioName) {
        this.studioName = studioName;
    }

    public void setCoverImageUrl(String coverImageUrl) {
        this.coverImageUrl = coverImageUrl;
    }

    public void setStoreFrontImageUrl(String storeFrontImageUrl) {
        this.storeFrontImageUrl = storeFrontImageUrl;
    }

    public void setDevLogUrl(String devLogUrl) {
        this.devLogUrl = devLogUrl;
    }

    public void setReviewUrl(String reviewUrl) {
        this.reviewUrl = reviewUrl;
    }

    public void setAppStoreReferences(Set<AppStoreReference> appStoreReferences) {
        this.appStoreReferences = appStoreReferences;
    }

    public void setScreenshots(Set<Screenshot> screenShots) {
        this.screenshots = screenShots;
    }
}
