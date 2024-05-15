// Copyright 2020-2024 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package com.dynamo.bob.util;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.text.ParseException;
import java.util.Enumeration;
import java.util.HashSet;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

import org.apache.commons.codec.binary.Base64;
import org.apache.commons.codec.binary.Hex;
import org.apache.commons.io.IOUtils;

public class LibraryUtil {

    /** Convert the supplied URL into a short string representation
     *
     * @param url Url of the library
     * @return the corresponding filename of the library on disk
     */
    public static String getHashedUrl(URL url) {
        // We hash the library URL and use it as a filename.
        // Previously we just replaced special characters with '_' and named it <URL>.zip,
        // this poses a problem on systems that couldn't handle arbitrarily large filenames.
        // Using a hash will also minimize leaking of sensitive data relative to the URL in
        // filenames (ex. if the URL contains username+password/authtoken).
        MessageDigest sha1;
        try {
            sha1 = MessageDigest.getInstance("SHA1");
        } catch (NoSuchAlgorithmException e) {
            throw new RuntimeException(e);
        }
        sha1.update(url.toString().getBytes());
        return new String(Hex.encodeHex(sha1.digest()));
    }

    public static String getETagFromName(String hashedUrl, String name)
    {
        Pattern libraryFileRe = Pattern.compile(hashedUrl + "-(.*)\\.zip");
        Matcher m = libraryFileRe.matcher(name);
        if (m.matches())
            return m.group(1);
        return null;
    }

    public static boolean matchUri(String hashedUrl, String name)
    {
        return getETagFromName(hashedUrl, name) != null;
    }

    public static String getFileName(URL url, String etag)
    {
        String etagB64 = new String(new Base64().encode(etag.getBytes()));
        return String.format("%s-%s.zip", getHashedUrl(url), etagB64);
    }

    /** Convert a list of library URLs into a map of corresponding files on disk.
     *
     * @param libPath base path of the library files
     * @param libUrls list of library URLs to convert
     * @return a map of corresponding files on disk
     */
    public static Map<String, File> collectLibraryFiles(String libPath, List<URL> libUrls) {
        File currentFiles[] = new File(libPath).listFiles(File::isFile);

        if (currentFiles == null && libUrls.size() > 0) {
            return null;
        }

        Map<String, File> libraries = new HashMap<String, File>();
        for (URL url : libUrls) {
            String hashedUrl = getHashedUrl(url);

            boolean matched = false;
            for (File f : currentFiles) {
                if (matchUri(hashedUrl, f.getName())) {
                    libraries.put(url.toString(), f);
                    matched = true;
                    break;
                }
            }
            if (!matched)
                libraries.put(url.toString(), null);
        }
        return libraries;
    }

    /** Find base directory path inside a zip archive from where all include dirs should be based.
    * Effectively searches for the first game.project since all include dirs are relative to this.
    *
    * @param archive archive in which to search for the game.project file
    * @return the string path to the directory where the game.project file was found
    */
    public static String findIncludeBaseDir(ZipFile archive) {
        String baseDir = "";
        // Need to get the base path, find first instance of game.project
        Enumeration<? extends ZipEntry> entries = archive.entries();
        while(entries.hasMoreElements()) {
            ZipEntry zipEntry = entries.nextElement();
            String entryPath = zipEntry.getName();
            if (entryPath.endsWith("/game.project")) {
                baseDir = entryPath.substring(0, entryPath.length() - "/game.project".length() + 1);
                break;
            }
        }
        return baseDir;
    }

    /**
     * Fetch the include dirs from the game.project file embedded in the specified archive.
     * The game.project is assumed to contain a comma separated list under the key library.include_dirs.
     *
     * @param archive archive in which to search for the game.project file
     * @return a set of include dir names
     * @throws IOException
     * @throws ParseException
     */
    public static Set<String> readIncludeDirsFromArchive(String includeBaseDir, ZipFile archive) throws IOException, ParseException {
        Set<String> includeDirs = new HashSet<String>();
        ZipEntry projectEntry = archive.getEntry(includeBaseDir + "game.project");

        if (projectEntry != null) {
            InputStream is = null;
            try {
                is = archive.getInputStream(projectEntry);
                BobProjectProperties properties = new BobProjectProperties();
                properties.load(is);
                String dirs = properties.getStringValue("library", "include_dirs", "");
                for (String dir : dirs.split("[,\\s]")) {
                    if (!dir.isEmpty()) {
                        includeDirs.add(dir);
                    }
                }
            } finally {
                IOUtils.closeQuietly(is);
            }
        }
        return includeDirs;
    }
}
