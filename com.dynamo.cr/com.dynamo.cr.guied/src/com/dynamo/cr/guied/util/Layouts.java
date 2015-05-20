package com.dynamo.cr.guied.util;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.List;

import org.apache.commons.io.IOUtils;
import org.eclipse.core.runtime.CoreException;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.render.proto.Render.DisplayProfile;
import com.dynamo.render.proto.Render.DisplayProfileQualifier;
import com.dynamo.render.proto.Render.DisplayProfiles;
import com.google.protobuf.TextFormat;


public class Layouts {
    private DisplayProfiles displayProfiles;
    private static Logger logger = LoggerFactory.getLogger(Layouts.class);

    public static class Layout {
        private String id;
        private float width;
        private float height;
        private float dpi;

        public Layout(String id, float width, float height, float dPI) {
            this.id = id;
            this.width = width;
            this.height = height;
            this.dpi = dPI;
        }

        public String getId() {
            return this.id;
        }

        public float getWidth() {
            return this.width;
        }

        public float getHeight() {
            return this.height;
        }

        public float getDpi() {
            return this.dpi;
        }
    }

    public static Layouts load(InputStream stream, float defaultWidth, float defaultHeight) throws CoreException, IOException {
        InputStreamReader reader = new InputStreamReader(stream);
        com.dynamo.render.proto.Render.DisplayProfiles.Builder builder = DisplayProfiles.newBuilder();

        DisplayProfile.Builder defaultProfile = DisplayProfile.newBuilder();
        defaultProfile.setName(GuiNodeStateBuilder.getDefaultStateId());
        DisplayProfileQualifier.Builder defaultQualifier = DisplayProfileQualifier.newBuilder();
        defaultQualifier.setWidth((int)defaultWidth);
        defaultQualifier.setHeight((int)defaultHeight);
        defaultProfile.addQualifiers(defaultQualifier);
        builder.addProfiles(defaultProfile);

        TextFormat.merge(reader, builder);
        Layouts layouts = new Layouts();
        layouts.displayProfiles = builder.build();
        IOUtils.closeQuietly(reader);
        return layouts;
    }

    public static Layout getLayout(Layouts layouts, String id) {
        List<DisplayProfile> profileList = layouts.displayProfiles.getProfilesList();
        for(DisplayProfile profile : profileList) {
            if(profile.getName().compareTo(id)==0)
            {
                DisplayProfileQualifier qualifier = profile.getQualifiers(0);
                Layout layout = new Layout(id, qualifier.getWidth(), qualifier.getHeight(), 0);
                return layout;
            }
        }
        return null;
    }

    public static Layout getDefaultLayout(Layouts layouts) {
        return getLayout(layouts, GuiNodeStateBuilder.getDefaultStateId());
    }

    public static List<Layout> getLayoutList(Layouts layouts) {
        List<DisplayProfile> profileList = layouts.displayProfiles.getProfilesList();
        List<Layout> layoutList = new ArrayList<Layout>(profileList.size());
        for(DisplayProfile profile : profileList) {
            DisplayProfileQualifier qualifier = profile.getQualifiers(0);
            layoutList.add(new Layout(profile.getName(), qualifier.getWidth(), qualifier.getHeight(), 0));
        }
        return layoutList;
    }

    public static List<String> getLayoutIds(Layouts layouts) {
        List<Layout> layoutList = getLayoutList(layouts);
        List<String> layoutStrings = new ArrayList<String>(layoutList.size());
        for(Layout layout : layoutList) {
            layoutStrings.add(layout.getId());
        }
        return layoutStrings;
    }

    public static String getMatchingLayout(Layouts layouts, List<String> idChoices, float width, float height, float dpi) {
        float match_area = width * height;
        float match_ratio = width / height;
        double match_distance = 3.0;
        String match_name = null;

        List<DisplayProfile> profileList = layouts.displayProfiles.getProfilesList();
        if(idChoices.isEmpty()) {
            for(DisplayProfile profile : profileList) {
                idChoices.add(profile.getName());
            }
        }

        for(DisplayProfile profile : profileList) {
            if(!idChoices.contains(profile.getName())) {
                continue;
            }
            List<DisplayProfileQualifier> qualifiersList = profile.getQualifiersList();
            for(DisplayProfileQualifier qualifier : qualifiersList) {

                float area = qualifier.getWidth() * qualifier.getHeight();
                float ratio = ((float)qualifier.getWidth()) / ((float)qualifier.getHeight());
                double distance = Math.abs(1.0 - (match_area / area)) + Math.abs(1.0 - (match_ratio / ratio));
                if(distance < match_distance) {
                    match_distance = distance;
                    match_name = profile.getName();
                }
            }
        }
        if(match_name == null) {
            if(profileList.isEmpty()) {
                logger.error("Invalid display profile setup, no profiles found.");
                return null;
            }
            return getMatchingLayout(layouts, new ArrayList<String>(), width, height, dpi);
        }
        return match_name;
    }

}


