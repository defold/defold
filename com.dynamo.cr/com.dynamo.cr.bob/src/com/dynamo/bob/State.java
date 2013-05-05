package com.dynamo.bob;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.Serializable;
import java.util.HashMap;
import java.util.Map;

/**
 * Bob state abstraction for persistent sha1-checksums
 * @author Christian Murray
 *
 */
public class State implements Serializable {

    private static final long serialVersionUID = -275410118302470803L;
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
