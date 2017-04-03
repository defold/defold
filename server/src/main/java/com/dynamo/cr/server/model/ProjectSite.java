package com.dynamo.cr.server.model;

import javax.persistence.*;
import java.util.*;
import java.util.stream.Collectors;

@Entity
@Table(name="project_sites")
public class ProjectSite {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column
    private String name;

    @Column(length = 10000)
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

    @Column
    private boolean playableUploaded;

    @Column
    private String playableImageUrl;

    @Column
    private String libraryUrl;

    @Column
    private String attachmentUrl;

    @Column
    private boolean publicSite;

    @Column
    private boolean showName = true;

    @Column
    private boolean allowComments = true;

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

    public List<Screenshot> getScreenshotsOrdered() {
        if (screenshots == null) {
            return Collections.emptyList();
        }
        return screenshots.stream()
                .sorted((o1, o2) -> {
                    if (o1.getSortOrder() == null) {
                        if (o2.getMediaType() == null) {
                            return 0;
                        }
                        return -1;
                    }
                    return Integer.compare(o1.getSortOrder(), o2.getSortOrder());
                })
                .collect(Collectors.toList());
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

    public boolean isPlayableUploaded() {
        return playableUploaded;
    }

    public void setPlayableUploaded(boolean playableUploaded) {
        this.playableUploaded = playableUploaded;
    }

    public String getPlayableImageUrl() {
        return playableImageUrl;
    }

    public void setPlayableImageUrl(String playableImageUrl) {
        this.playableImageUrl = playableImageUrl;
    }

    public String getLibraryUrl() {
        return libraryUrl;
    }

    public void setLibraryUrl(String libraryUrl) {
        this.libraryUrl = libraryUrl;
    }

    public String getAttachmentUrl() {
        return attachmentUrl;
    }

    public void setAttachmentUrl(String attachmentUrl) {
        this.attachmentUrl = attachmentUrl;
    }

    public boolean isPublicSite() {
        return publicSite;
    }

    public void setPublicSite(boolean publicSite) {
        this.publicSite = publicSite;
    }

    public boolean isShowName() {
        return showName;
    }

    public void setShowName(boolean showName) {
        this.showName = showName;
    }

    public boolean isAllowComments() {
        return allowComments;
    }

    public void setAllowComments(boolean allowComments) {
        this.allowComments = allowComments;
    }
}
