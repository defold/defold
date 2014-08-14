package com.dynamo.bob.fs;

public class ResourceUtil {

    /**
     * Change extension of filename
     * @param fileName file-name to change extension for
     * @param ext new extension including preceding dot
     * @return new file-name
     */
    public static String changeExt(String fileName, String ext) {
        int i = fileName.lastIndexOf(".");
        if (i == -1) {
            throw new IllegalArgumentException(String.format("Missing extension in name '%s'", fileName));
        }
        fileName = fileName.substring(0, i);
        return fileName + ext;
    }
}
