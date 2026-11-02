// Copyright 2020-2026 The Defold Foundation
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

package com.dynamo.bob;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.Serializable;
import java.util.HashMap;
import java.util.Map;
import java.util.List;
import java.util.ArrayList;

import com.dynamo.bob.fs.IResource;

/**
 * Bob state abstraction for persistent sha1-checksums
 * @author Christian Murray
 *
 */
public class State implements Serializable {

    private static final long serialVersionUID = -275410118302470802L + 1;
    private Map<String, byte[]> signatures = new HashMap<String, byte[]>();

    /**
     * Get signature for path
     * @param path path to get sha1 for
     * @return signature or null of no mapping exists
     */
    public byte[] getSignature(String path) {
        return signatures.get(path);
    }

    /**
     * Add signature
     * @param path path to set sha1 for
     * @param signature signature to set
     */
    public void putSignature(String path, byte[] signature) {
        signatures.put(path, signature);
    }

    /**
     * Remove signature
     * @param path path to set sha1 for
     */
    public void removeSignature(String path) {
        signatures.remove(path);
    }

    /**
     * Get all registered paths
     * @return list of all registered paths
     */
    public List<String> getPaths() {
        return new ArrayList<>(signatures.keySet());
    }

    /**
     * Load state from resource
     * @param resource state resource
     * @return {@link State}
     * @throws IOException
     */
    public static State load(IResource resource) throws IOException {
        byte[] content = resource.getContent();
        if (content == null) {
            return new State();
        } else {
            try {
                ObjectInputStream is = new ObjectInputStream(new ByteArrayInputStream(content));
                State state = (State) is.readObject();
                return state;
            } catch (Throwable e) {
                System.err.println("Unable to load state");
                e.printStackTrace();
                return new State();
            }
        }
    }

    /**
     * Save state
     * @param resource state resource
     * @throws IOException
     */
    public void save(IResource resource) throws IOException {
        ByteArrayOutputStream bos = new ByteArrayOutputStream(128 * 1024);
        ObjectOutputStream os = new ObjectOutputStream(bos);
        os.writeObject(this);
        os.close();
        resource.setContent(bos.toByteArray());
    }

}
