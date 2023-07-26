// Copyright 2020-2023 The Defold Foundation
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

package com.dynamo.bob.archive;

import java.util.Map;
import java.util.HashMap;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.archive.ManifestBuilder;
import com.dynamo.bob.logging.Logger;

public class ResourceDigestCache {

    private static Logger logger = Logger.getLogger(ResourceDigestCache.class.getName());

    private static Map<String, byte[]> cache = new HashMap<>();
    private static Map<String, MessageDigest> digests = new HashMap<>();

    public static void update(String path, byte[] input) throws CompileExceptionError {
        MessageDigest digest = digests.get(path);
        if (digest == null) {
            try {
                digest = ManifestBuilder.create().getResourceHashDigest();
            }
            catch (NoSuchAlgorithmException e) {
                throw new CompileExceptionError(e);
            }
            digests.put(path, digest);
        }
        digest.update(input);
    }

    public static void update(IResource resource, byte[] input) throws CompileExceptionError {
        update(resource.getAbsPath(), input);
    }

    public static byte[] digest(String path) throws CompileExceptionError {
        byte[] b = cache.get(path);
        if (b == null) {
            MessageDigest digest = digests.get(path);
            if (digest != null) {
                b = digest.digest();
                cache.put(path, b);
            }
        }
        return b;
    }

    public static byte[] digest(IResource resource) throws CompileExceptionError {
        return digest(resource.getAbsPath());
    }

    public static boolean hasDigest(String path) {
        return digests.containsKey(path);
    }

    public static boolean hasDigest(IResource resource) {
        return hasDigest(resource.getAbsPath());
    }
}