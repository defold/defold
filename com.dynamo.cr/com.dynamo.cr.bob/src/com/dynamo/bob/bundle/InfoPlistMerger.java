
package com.dynamo.bob.bundle;

import java.io.*;
import java.lang.Boolean;
import java.util.Iterator;
import java.util.ArrayList;
import java.util.Arrays;
import java.math.BigInteger;
import java.math.BigDecimal;
import java.util.logging.Level;
import java.util.logging.Logger;

// https://commons.apache.org/proper/commons-configuration/javadocs/v1.10/apidocs/index.html?org/apache/commons/configuration/FileConfiguration.html
import org.apache.commons.configuration.ConfigurationException;
import org.apache.commons.configuration.plist.XMLPropertyListConfiguration;

class PlistMergeException extends Exception {
    PlistMergeException(String message) {
        super(message);
    }
}

public class InfoPlistMerger {
    private static Logger logger;

    InfoPlistMerger(Logger logger) {
        InfoPlistMerger.logger = logger;
    }

    // Merges the lib onto base, using the merging rules from https://developer.android.com/studio/build/manifest-merge.html
    public static void mergePlists(XMLPropertyListConfiguration base, XMLPropertyListConfiguration lib) throws PlistMergeException {
        Iterator<String> it = lib.getKeys();
        while (it.hasNext()) {
            String key = it.next();

            Object baseValue = null;
            if (base.containsKey(key)) {
                baseValue = base.getProperty(key);
            }

            Object libValue = lib.getProperty(key);

            if (baseValue == null) {
                base.addProperty(key, libValue);
            } else {

                if (baseValue.getClass().equals(libValue.getClass())) {

                    if (!baseValue.getClass().equals(ArrayList.class)) {
                        boolean differ = false;
                        if (baseValue.getClass().equals(byte[].class)) {
                            if (!Arrays.equals((byte[])baseValue, (byte[])libValue)) {
                                differ = true;
                            }
                        } else if (!baseValue.equals(libValue)) {
                            differ = true;
                        }
                        if (differ) {
                            InfoPlistMerger.logger.log(Level.WARNING, String.format("Plist overriding value for key '%s': from '%s' to '%s'", key, baseValue.toString(), libValue.toString()));
                            base.addProperty(key, libValue);
                        }
                    }

                    if (baseValue.getClass().equals(String.class)) {
                    }
                    else if (baseValue.getClass().equals(BigInteger.class)) {
                    }
                    else if (baseValue.getClass().equals(BigDecimal.class)) {
                    }
                    else if (baseValue.getClass().equals(Boolean.class)) {
                    }
                    else if (baseValue.getClass().equals(byte[].class)) {
                    }
                    else if (baseValue.getClass().equals(ArrayList.class)) {
                        ArrayList baseArray = (ArrayList)baseValue;
                        ArrayList libArray = (ArrayList)libValue;
                        baseArray.addAll(libArray);
                    }
                    else {
                        throw new PlistMergeException(String.format("Plist contains unknown type for key '%s': %s", key, baseValue.getClass().getName()));
                    }

                } else { // if the value classes differ, then raise an error!
                    throw new PlistMergeException(String.format("Plist contains conflicting types for key '%s': %s vs %s", key, baseValue.getClass().getName(), libValue.getClass().getName()));
                }
            }
        }
    }

    private static XMLPropertyListConfiguration loadPlist(File file) {
        try {
            XMLPropertyListConfiguration plist = new XMLPropertyListConfiguration();
            plist.load(file);
            return plist;
        } catch (ConfigurationException e) {
            throw new RuntimeException(String.format("Failed to parse plist '%s': %s", file.getAbsolutePath(), e.toString()));
        }
    }

    public static void merge(File main, File[] libraries, File out) throws RuntimeException {
        XMLPropertyListConfiguration basePlist = loadPlist(main);

        // For error reporting/troubleshooting
        String paths = "\n" + main.getAbsolutePath();

        for (File library : libraries) {
            paths += "\n" + library.getAbsolutePath();
            XMLPropertyListConfiguration libraryPlist = loadPlist(library);
            try {
                mergePlists(basePlist, libraryPlist);
            } catch (PlistMergeException e) {
                throw new RuntimeException(String.format("Errors merging plists: %s:\n%s", paths, e.toString()));
            }
        }

        try {
            basePlist.save(out);
        } catch (ConfigurationException e) {
            throw new RuntimeException("Failed to parse plist: " + e.toString());
        }
    }
}
