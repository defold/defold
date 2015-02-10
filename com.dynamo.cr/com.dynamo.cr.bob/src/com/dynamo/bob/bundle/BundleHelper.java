package com.dynamo.bob.bundle;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.StringWriter;
import java.net.MalformedURLException;
import java.net.URL;
import java.text.ParseException;
import java.util.HashMap;
import java.util.Map;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.IOUtils;

import com.dynamo.bob.Bob;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Project;
import com.dynamo.bob.util.BobProjectProperties;
import com.samskivert.mustache.Mustache;
import com.samskivert.mustache.Template;

public class BundleHelper {
    private Project project;
    private String title;
    private File buildDir;
    private File appDir;
    private Map<String, Map<String, Object>> propertiesMap;

    public BundleHelper(Project project, Platform platform, File bundleDir, String appDirSuffix) throws IOException {
        BobProjectProperties projectProperties = project.getProjectProperties();

        this.project = project;
        this.title = projectProperties.getStringValue("project", "title", "Unnamed");

        this.buildDir = new File(project.getRootDirectory(), project.getBuildDirectory());
        this.appDir = new File(bundleDir, title + appDirSuffix);

        this.propertiesMap = createPropertiesMap(project.getProjectProperties());
    }

    static Object convert(String value, String type) {
        if (type != null && type.equals("bool")) {
            if (value != null) {
                return value.equals("1");
            } else {
                return false;
            }
        }
        return value;
    }

    public static Map<String, Map<String, Object>> createPropertiesMap(BobProjectProperties projectProperties) throws IOException {
        BobProjectProperties meta = new BobProjectProperties();
        InputStream is = Bob.class.getResourceAsStream("meta.properties");
        try {
            meta.load(is);
        } catch (ParseException e) {
            throw new RuntimeException("Failed to parse meta.properties", e);
        } finally {
            IOUtils.closeQuietly(is);
        }

        Map<String, Map<String, Object>> map = new HashMap<>();

        for (String c : meta.getCategoryNames()) {
            map.put(c, new HashMap<String, Object>());

            for (String k : meta.getKeys(c)) {
                if (k.endsWith(".default")) {
                    String k2 = k.split("\\.")[0];
                    String v = meta.getStringValue(c, k);
                    Object v2 = convert(v, meta.getStringValue(c, k2 + ".type"));
                    map.get(c).put(k2, v2);
                }
            }
        }

        for (String c : projectProperties.getCategoryNames()) {
            if (!map.containsKey(c)) {
                map.put(c, new HashMap<String, Object>());
            }

            for (String k : projectProperties.getKeys(c)) {
                String def = meta.getStringValue(c, k + ".default");
                map.get(c).put(k, def);
                String v = projectProperties.getStringValue(c, k);
                Object v2 = convert(v, meta.getStringValue(c, k + ".type"));
                map.get(c).put(k, v2);
            }
        }

        return map;
    }

    URL getResource(String category, String key, String defaultValue) {
        BobProjectProperties projectProperties = project.getProjectProperties();
        File projectRoot = new File(project.getRootDirectory());
        String s = projectProperties.getStringValue(category, key);
        if (s != null && s.trim().length() > 0) {
            try {
                return new File(projectRoot, s).toURI().toURL();
            } catch (MalformedURLException e) {
                throw new RuntimeException(e);
            }
        } else {
            return getClass().getResource(defaultValue);
        }
    }

    public BundleHelper format(Map<String, Object> properties, String templateCategory, String templateKey, String defaultTemplate, File toFile) throws IOException {
        URL templateURL = getResource(templateCategory, templateKey, defaultTemplate);
        Template template = Mustache.compiler().compile(IOUtils.toString(templateURL));
        StringWriter sw = new StringWriter();
        template.execute(this.propertiesMap, properties, sw);
        sw.flush();
        FileUtils.write(toFile, sw.toString());
        return this;
    }

    public BundleHelper format(String templateCategory, String templateKey, String defaultTemplate, File toFile) throws IOException {
        return format(new HashMap<String, Object>(), templateCategory, templateKey, defaultTemplate, toFile);
    }


    public BundleHelper copyBuilt(String name) throws IOException {
        FileUtils.copyFile(new File(buildDir, name), new File(appDir, name));
        return this;
    }

}