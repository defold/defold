package com.dynamo.bob.bundle;

/*
Assets.xcassets
    .DS_Store
    Contents.json (1)
    LaunchImage.launchimage
        Contents.json (2)
        <launchimage_name>.png
    Icons.brandassets
        Contents.json (3)
        large.imagestack
            Contents.json (4)
            1.imagestacklayer (repeats for each layer, min 2, max 5)
                Contents.json (5)
                Content.imageset
                    Contents.json (6)
                    <layer_image_name>.png
        small.imagestack
            Contents.json
            1.imagestacklayer (repeats for each layer, min 2, max 5)
                Contents.json (5)
                Content.imageset
                    Contents.json (6)
                    <layer_image_name>.png
        top.imageset
            Contents.json (7)
            <topshelf_image_name>.png
*/


import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.json.simple.JSONArray;
import org.json.simple.JSONObject;

import com.dynamo.bob.util.BobProjectProperties;
import com.dynamo.bob.util.Exec;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

public class TVOSImagesHelper {
    private BobProjectProperties projectProperties;
    private String projectRoot;
    File appDir;

    public enum AppIcon {
        SMALL,
        LARGE
    }

    public enum AppImage {
        TOP_SHELF,
        LAUNCH
    }

    public TVOSImagesHelper(BobProjectProperties projectProperties, String projectRoot, File appDir) {
        this.projectProperties = projectProperties;
        this.projectRoot = projectRoot;
        this.appDir = appDir;
    }

    private JSONObject getBaseJson() {
        JSONObject jo = new JSONObject();
        JSONObject joInfo = new JSONObject();
        joInfo.put("version", new Integer(1));
        joInfo.put("author", "defold");
        jo.put("info", joInfo);

        return jo;
    }

    private void writeJson(File destFile, JSONObject json, boolean append) {
        try {
            FileUtils.write(destFile, json.toJSONString(), append);
        } catch(IOException ioe) {
            ioe.printStackTrace();
        }
    }

    private boolean hasIconLayer(AppIcon type, int layer) {
        String size = (type.equals(AppIcon.LARGE) ? "1280x768" : "400x240");
        String res = projectProperties.getStringValue("tvos", "app_icon_" + size + "_layer_" + layer);
        if (res != null && res.length() > 0) {
            return true;
        }

        return false;
    }

    private boolean hasAnyIconLayer(AppIcon type) {
        for(int layer = 1; layer <= 5; layer++) {
            if (hasIconLayer(type, layer)) {
                return true;
            }
        }

        return false;
    }

    private boolean hasImage(AppImage type) {
        String name = (type.equals(AppImage.TOP_SHELF) ? "app_icon_1920x720" : "launch_image_1920x1080");
        String res = projectProperties.getStringValue("tvos",name);
        if (res != null && res.length()>0) {
            return true;
        }

        return false;
    }

    private void createImageSet(File parentDir, String dirName, String imageFile, boolean isLaunchImage) throws IOException {
        File imgSetDir = new File(parentDir.getAbsolutePath() + File.separator + dirName);
        imgSetDir.mkdirs();

        File png = new File(imageFile);

        // Contents.json
        JSONObject contentsJson = getBaseJson();
        JSONArray arr = new JSONArray();
        JSONObject img = new JSONObject();
        img.put("idiom", "tv");
        img.put("filename", FilenameUtils.getName(png.getAbsolutePath()));
        img.put("scale", "1x");
        if(isLaunchImage) {
            img.put("orientation", "landscape");
            img.put("extent", "full-screen");
            img.put("minimum-system-version", "9.0");
        }
        arr.add(img);
        contentsJson.put("images", arr);
        File contentsFile = new File(imgSetDir.getAbsolutePath() + File.separator + "Contents.json");
        writeJson(contentsFile, contentsJson, false);

        // Image
        // Handled later...
        File inFile = new File(projectRoot, imageFile);
        File outFile = new File(imgSetDir.getAbsolutePath(), FilenameUtils.getName(png.getAbsolutePath()));
        FileUtils.copyFile(inFile, outFile);
    }

    private void createImageStackLayer(File parentDir, String dirName, String imageFile) throws IOException {
        File imgStackLayerDir = new File(parentDir.getAbsolutePath() + File.separator + dirName);
        imgStackLayerDir.mkdirs();
        // Contents.json
        JSONObject contentsJson = getBaseJson();
        File contentsFile = new File(imgStackLayerDir.getAbsolutePath() + File.separator + "Contents.json");
        writeJson(contentsFile, contentsJson, false);

        // Contents.imageset dir
        createImageSet(imgStackLayerDir, "Content.imageset", imageFile, false);
    }

    private void createImageStack(File rootDir, String imageBaseName, List<String> imageFiles) throws IOException {
        File imgStack = new File(rootDir.getAbsolutePath() + File.separator + imageBaseName + ".imagestack");
        imgStack.mkdirs();

        // Contents.json
        JSONObject contentsJson = getBaseJson();
        JSONArray layers = new JSONArray();

        for(int i=1; i<=imageFiles.size(); i++) {
            JSONObject filenameJson = new JSONObject();
            filenameJson.put("filename", i + ".imagestacklayer");
            layers.add(filenameJson);
            createImageStackLayer(imgStack, i + ".imagestacklayer", imageFiles.get(i - 1));
        }
        contentsJson.put("layers", layers);
        File contentsJsonFile = new File(imgStack.getAbsolutePath() + File.separator + "Contents.json");
        writeJson(contentsJsonFile, contentsJson, false);
    }

    public int createAssetsCar() throws IOException {
        // If we have no icons or images at all, return...
        if(!hasAnyIconLayer(AppIcon.LARGE) &&
           !hasAnyIconLayer(AppIcon.SMALL) &&
           !hasImage(AppImage.TOP_SHELF) &&
           !hasImage(AppImage.LAUNCH)) {
            return 0;
        }
        String title = projectProperties.getStringValue("project", "title", "Unnamed");

        File root = new File(FileUtils.getTempDirectoryPath() + File.separator + title);
        if(root.exists()) {
            try {
                FileUtils.deleteDirectory(root);
            } catch (IOException e) {
            }
        }
        root.mkdirs();
        //System.out.println("ASSETS DIR:" + root.getAbsolutePath());
        // Create Assets.xcassets dir
        File xcassetsDir = new File(root.getAbsolutePath() + File.separator + "Assets.xcassets");
        xcassetsDir.mkdirs();

        // Create root Contents.json (base info json)
        JSONObject xcassetsContentsJson = getBaseJson();
        File xcassetsContentsJsonFile = new File(xcassetsDir.getAbsolutePath() + File.separator + "Contents.json");
        writeJson(xcassetsContentsJsonFile, xcassetsContentsJson, false);

        // Create Icons.brandassets dir
        File iconsDir = new File(xcassetsDir.getAbsolutePath() + File.separator + "Icons.brandassets");
        iconsDir.mkdirs();

        // Icons.brandassets Contents.json
        JSONObject iconsContentsJson = getBaseJson();
        JSONArray assetsArr = new JSONArray();

        // Create large App Icon
        if(hasAnyIconLayer(AppIcon.LARGE)) {
            JSONObject itm = new JSONObject();
            itm.put("size", "1280x768");
            itm.put("idiom", "tv");
            itm.put("filename", "large.imagestack");
            itm.put("role", "primary-app-icon");
            assetsArr.add(itm);
            List<String> imageFiles = new ArrayList<String>();
            for(int layer=1; layer<=5; layer++) {
                if (hasIconLayer(AppIcon.LARGE, layer)) {
                    imageFiles.add(projectProperties.getStringValue("tvos","app_icon_1280x768_layer_" + layer));
                }
            }
            createImageStack(iconsDir, "large", imageFiles);
        }

        // Create small App Icon
        if(hasAnyIconLayer(AppIcon.SMALL)) {
            JSONObject itm = new JSONObject();
            itm.put("size", "400x240");
            itm.put("idiom", "tv");
            itm.put("filename", "small.imagestack");
            itm.put("role", "primary-app-icon");
            assetsArr.add(itm);
            List<String> imageFiles = new ArrayList<String>();
            for(int layer=1; layer<=5; layer++) {
                if (hasIconLayer(AppIcon.SMALL, layer)) {
                    imageFiles.add(projectProperties.getStringValue("tvos","app_icon_400x240_layer_" + layer));
                }
            }
            createImageStack(iconsDir, "small", imageFiles);
        }

        // Create Top Shelf Image
        if(hasImage(AppImage.TOP_SHELF)) {
            JSONObject itm = new JSONObject();
            itm.put("size", "1920x720");
            itm.put("idiom", "tv");
            itm.put("filename", "top.imageset");
            itm.put("role", "top-shelf-image");
            assetsArr.add(itm);
            createImageSet(iconsDir, "top.imageset", projectProperties.getStringValue("tvos","app_icon_1920x720"), false);
        }

        iconsContentsJson.put("assets", assetsArr);
        File iconsContentsJsonFile = new File(iconsDir.getAbsolutePath() + File.separator + "Contents.json");
        writeJson(iconsContentsJsonFile, iconsContentsJson, false);

        // Create Launch Image
        if(hasImage(AppImage.LAUNCH)) {
            createImageSet(xcassetsDir, "LaunchImage.launchimage", projectProperties.getStringValue("tvos","launch_image_1920x1080"), true);
        }

        // Run actool to create Assets.car in appDir
        // actool --output-format human-readable-text --notices --warnings --output-partial-info-plist kaka/partial.plist --app-icon "Icons"
        //   --launch-image LaunchImage --compress-pngs --target-device tv --minimum-deployment-target 9.0 --platform appletvos --compile kaka/ Assets.xcassets/
        int result = Exec.exec("/Applications/Xcode.app/Contents/Developer/usr/bin/actool",
                "--output-format", "human-readable-text",
                "--notices",
                "--warnings",
                "--errors",
                "--output-partial-info-plist", xcassetsDir.getAbsolutePath() + File.separator + "IconsAndImages.plist",
                "--app-icon", "Icons",
                "--launch-image", "LaunchImage",
                "--compress-pngs",
                "--target-device", "tv",
                "--minimum-deployment-target", "9.0",
                "--platform", "appletvos",
                "--compile", appDir.getAbsolutePath(), xcassetsDir.getAbsolutePath());

        return result;
    }
}
