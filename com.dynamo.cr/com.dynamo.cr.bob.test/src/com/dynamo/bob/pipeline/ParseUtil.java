package com.dynamo.bob.pipeline;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.IResource;
import com.dynamo.gameobject.proto.GameObject.CollectionDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.dynamo.lua.proto.Lua.LuaModule;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.Message;

public class ParseUtil {

    private interface IParser {
        Message parse(byte[] content) throws InvalidProtocolBufferException;
    }

    private static Map<String, IParser> parseMap = new HashMap<String, IParser>();

    static {
        parseMap.put("collectionc", new IParser() {
            @Override
            public Message parse(byte[] content) throws InvalidProtocolBufferException {
                return CollectionDesc.parseFrom(content);
            }
        });
        parseMap.put("scriptc", new IParser() {
            @Override
            public Message parse(byte[] content) throws InvalidProtocolBufferException {
                return LuaModule.parseFrom(content);
            }
        });
        parseMap.put("goc", new IParser() {
            @Override
            public Message parse(byte[] content) throws InvalidProtocolBufferException {
                return PrototypeDesc.parseFrom(content);
            }
        });
    }

    public static Message parse(IResource resource) throws IOException, InvalidProtocolBufferException {
        String ext = FilenameUtils.getExtension(resource.getPath());
        IParser parser = parseMap.get(ext);
        if (parser == null) {
            throw new RuntimeException("No parser registered for extension " + ext);
        }
        return parser.parse(resource.getContent());
    }
}
