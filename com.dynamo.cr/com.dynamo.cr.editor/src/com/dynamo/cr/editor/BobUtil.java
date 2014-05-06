package com.dynamo.cr.editor;

import java.util.HashMap;
import java.util.Map;

import org.apache.commons.codec.binary.Base64;
import org.apache.commons.lang.SerializationUtils;

public class BobUtil {

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
}
