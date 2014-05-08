package com.dynamo.cr.editor;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.apache.commons.codec.binary.Base64;
import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;
import org.apache.commons.lang.SerializationUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.bob.Bob;
import com.dynamo.cr.editor.core.ProjectProperties;

public class BobUtil {

    private static Logger logger = LoggerFactory
            .getLogger(BobUtil.class);

    /**
     * Retrieve bob specific args from the supplied args map.
     * Bob args is a serialized HashMap stored with key bobArgs.
     */
    @SuppressWarnings("unchecked")
    public static final Map<String, String> getBobArgs(Map<String, String> args) {
        String bobArgsEncoded = args.get("bobArgs");
        if (bobArgsEncoded != null) {
            return (Map<String, String>) SerializationUtils.deserialize(Base64.decodeBase64(bobArgsEncoded));
        }
        return null;
    }

    /**
     * Insert bob specific args into the supplied args map.
     * Bob args is a serialized HashMap stored with key bobArgs.
     */
    public static final void putBobArgs(HashMap<String, String> bobArgs, Map<String, String> dstArgs) {
        String bobArgsEncoded = Base64.encodeBase64String(SerializationUtils.serialize(bobArgs));
        dstArgs.put("bobArgs", bobArgsEncoded);
    }

    public static List<URL> getLibraryUrls(String rootPath) throws IOException {
        List<URL> urls = new ArrayList<URL>();
        ProjectProperties properties = new ProjectProperties();
        File projectProps = new File(FilenameUtils.concat(rootPath, "game.project"));
        InputStream input = null;
        try {
            input = new BufferedInputStream(new FileInputStream(projectProps));
            properties.load(input);
        } catch (Exception e) {
            logger.warn("Failed to parse game.project");
        } finally {
            IOUtils.closeQuietly(input);
        }
        String dependencies = properties.getStringValue("project", "dependencies", null);
        if (dependencies != null) {
            urls = Bob.parseLibraryUrls(dependencies);
        }
        return urls;
    }
}
