package com.dynamo.bob.pipeline;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.fs.IResource;
import com.dynamo.gameobject.proto.GameObject.CollectionDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.dynamo.graphics.proto.Graphics.TextureImage;
import com.dynamo.lua.proto.Lua.LuaModule;
import com.dynamo.render.proto.Font;
import com.dynamo.spine.proto.Spine;
import com.dynamo.sprite.proto.Sprite.SpriteDesc;
import com.dynamo.textureset.proto.TextureSetProto.TextureSet;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;
import com.google.protobuf.TextFormat.ParseException;

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
        parseMap.put("spritec", new IParser() {
            @Override
            public Message parse(byte[] content) throws InvalidProtocolBufferException {
                return SpriteDesc.parseFrom(content);
            }
        });
        parseMap.put("texturec", new IParser() {
            @Override
            public Message parse(byte[] content) throws InvalidProtocolBufferException {
                return TextureImage.parseFrom(content);
            }
        });
        parseMap.put("texturesetc", new IParser() {
            @Override
            public Message parse(byte[] content) throws InvalidProtocolBufferException {
                return TextureSet.parseFrom(content);
            }
        });
        parseMap.put("goc", new IParser() {
            @Override
            public Message parse(byte[] content) throws InvalidProtocolBufferException {
                return PrototypeDesc.parseFrom(content);
            }
        });
        parseMap.put("go", new IParser() {
            @Override
            public Message parse(byte[] content) throws InvalidProtocolBufferException {
                PrototypeDesc.Builder builder = PrototypeDesc.newBuilder();
                try {
                    TextFormat.merge(new String(content), builder);
                } catch (ParseException e) {
                    throw new RuntimeException(e);
                }
                return builder.build();
            }
        });
        parseMap.put("sprite", new IParser() {
            @Override
            public Message parse(byte[] content) throws InvalidProtocolBufferException {
                SpriteDesc.Builder builder = SpriteDesc.newBuilder();
                try {
                    TextFormat.merge(new String(content), builder);
                } catch (ParseException e) {
                    throw new RuntimeException(e);
                }
                return builder.build();
            }
        });
        parseMap.put("spinescenec", new IParser() {
            @Override
            public Message parse(byte[] content) throws InvalidProtocolBufferException {
                return Spine.SpineScene.parseFrom(content);
            }
        });
        parseMap.put("spinemodelc", new IParser() {
            @Override
            public Message parse(byte[] content) throws InvalidProtocolBufferException {
                return Spine.SpineModelDesc.parseFrom(content);
            }
        });
        parseMap.put("fontc", new IParser() {
            @Override
            public Message parse(byte[] content) throws InvalidProtocolBufferException {
                return Font.FontMap.parseFrom(content);
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
