package com.dynamo.cr.server.resources.mappers;

import com.dynamo.cr.protocol.proto.Protocol;
import com.dynamo.cr.server.model.AppStoreReference;
import com.dynamo.cr.server.model.ProjectSite;
import com.dynamo.cr.server.model.Screenshot;

import java.util.List;
import java.util.Set;

public class ProjectSiteMapper {

    public static Protocol.ProjectSiteList map(List<ProjectSite> projectSites) {
        Protocol.ProjectSiteList.Builder builder = Protocol.ProjectSiteList.newBuilder();

        for (ProjectSite projectSite : projectSites) {
            builder.addSites(map(projectSite));
        }

        return builder.build();
    }

    public static Protocol.ProjectSite map(ProjectSite projectSite) {
        Protocol.ProjectSite.Builder builder = Protocol.ProjectSite.newBuilder();

        if (projectSite != null) {

            if (projectSite.getName() != null) {
                builder.setName(projectSite.getName());
            }

            if (projectSite.getDescription() != null) {
                builder.setDescription(projectSite.getDescription());
            }

            if (projectSite.getStudioUrl() != null) {
                builder.setStudioUrl(projectSite.getStudioUrl());
            }

            if (projectSite.getStudioName() != null) {
                builder.setStudioName(projectSite.getStudioName());
            }

            if (projectSite.getCoverImageUrl() != null) {
                builder.setCoverImageUrl(projectSite.getCoverImageUrl());
            }

            if (projectSite.getStoreFrontImageUrl() != null) {
                builder.setStoreFrontImageUrl(projectSite.getStoreFrontImageUrl());
            }

            if (projectSite.getDevLogUrl() != null) {
                builder.setDevLogUrl(projectSite.getDevLogUrl());
            }

            if (projectSite.getReviewUrl() != null) {
                builder.setReviewUrl(projectSite.getReviewUrl());
            }

            Set<AppStoreReference> appStoreReferences = projectSite.getAppStoreReferences();
            if (appStoreReferences != null) {
                for (AppStoreReference appStoreReference : appStoreReferences) {
                    Protocol.ProjectSite.AppStoreReference.Builder appStoreReferenceBuilder = Protocol.ProjectSite.AppStoreReference.newBuilder();
                    appStoreReferenceBuilder.setId(appStoreReference.getId());
                    appStoreReferenceBuilder.setLabel(appStoreReference.getLabel());
                    appStoreReferenceBuilder.setUrl(appStoreReference.getUrl());
                    builder.addAppStoreReferences(appStoreReferenceBuilder.build());
                }
            }

            Set<Screenshot> screenShots = projectSite.getScreenshots();
            if (screenShots != null) {
                for (Screenshot screenShot : screenShots) {
                    Protocol.ProjectSite.Screenshot.Builder screenShotBuilder = Protocol.ProjectSite.Screenshot.newBuilder();
                    screenShotBuilder.setId(screenShot.getId());
                    screenShotBuilder.setUrl(screenShot.getUrl());
                    screenShotBuilder.setMediaType(Protocol.ScreenshotMediaType.valueOf(screenShot.getMediaType().name()));
                    builder.addScreenshots(screenShotBuilder.build());
                }
            }
        }

        return builder.build();
    }
}
