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

package com.dynamo.bob.pipeline;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.HashMap;
import java.util.Map;

import com.dynamo.bob.util.TextureUtil;
import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.fs.IResource;
import com.dynamo.gameobject.proto.GameObject.CollectionDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.dynamo.gamesys.proto.GameSystem.FactoryDesc;
import com.dynamo.gamesys.proto.GameSystem.CollectionFactoryDesc;
import com.dynamo.gamesys.proto.Gui;
import com.dynamo.gamesys.proto.ModelProto;
import com.dynamo.gamesys.proto.Sprite.SpriteDesc;
import com.dynamo.gamesys.proto.Sound.SoundDesc;
import com.dynamo.gamesys.proto.TextureSetProto.TextureSet;
import com.dynamo.graphics.proto.Graphics.TextureImage;
import com.dynamo.graphics.proto.Graphics;
import com.dynamo.lua.proto.Lua.LuaModule;
import com.dynamo.render.proto.Font;
import com.dynamo.render.proto.Material;
import com.dynamo.render.proto.Compute;
import com.dynamo.render.proto.Render.RenderPrototypeDesc;
import com.dynamo.rig.proto.Rig;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;
import com.google.protobuf.TextFormat.ParseException;

public class ParseUtil {

    public interface IParser {
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
        parseMap.put("render_scriptc", new IParser() {
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
        parseMap.put("soundc", new IParser() {
            @Override
            public Message parse(byte[] content) throws InvalidProtocolBufferException {
                return SoundDesc.parseFrom(content);
            }
        });
        parseMap.put("oggc", new IParser() {
            @Override
            public Message parse(byte[] content) throws InvalidProtocolBufferException {
                return null;
            }
        });
        parseMap.put("opusc", new IParser() {
            @Override
            public Message parse(byte[] content) throws InvalidProtocolBufferException {
                return null;
            }
        });
        parseMap.put("texturec", new IParser() {
            @Override
            public Message parse(byte[] content) throws InvalidProtocolBufferException {
                return TextureUtil.textureResourceBytesToTextureImage(content);
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
        parseMap.put("sound", new IParser() {
            @Override
            public Message parse(byte[] content) throws InvalidProtocolBufferException {
                SoundDesc.Builder builder = SoundDesc.newBuilder();
                try {
                    TextFormat.merge(new String(content), builder);
                } catch (ParseException e) {
                    throw new RuntimeException(e);
                }
                return builder.build();
            }
        });
        parseMap.put("skeletonc", new IParser() {
            @Override
            public Message parse(byte[] content) throws InvalidProtocolBufferException {
                return Rig.Skeleton.parseFrom(content);
            }
        });
        parseMap.put("meshsetc", new IParser() {
            @Override
            public Message parse(byte[] content) throws InvalidProtocolBufferException {
                return Rig.MeshSet.parseFrom(content);
            }
        });
        parseMap.put("animationsetc", new IParser() {
            @Override
            public Message parse(byte[] content) throws InvalidProtocolBufferException {
                return Rig.AnimationSet.parseFrom(content);
            }
        });
        parseMap.put("rigscenec", new IParser() {
            @Override
            public Message parse(byte[] content) throws InvalidProtocolBufferException {
                return Rig.RigScene.parseFrom(content);
            }
        });
        parseMap.put("modelc", new IParser() {
            @Override
            public Message parse(byte[] content) throws InvalidProtocolBufferException {
                return ModelProto.Model.parseFrom(content);
            }
        });
        parseMap.put("spc", new IParser() {
            @Override
            public Message parse(byte[] content) throws InvalidProtocolBufferException {
                return Graphics.ShaderDesc.parseFrom(content);
            }
        });
        parseMap.put("fontc", new IParser() {
            @Override
            public Message parse(byte[] content) throws InvalidProtocolBufferException {
                return Font.FontMap.parseFrom(content);
            }
        });
        parseMap.put("glyph_bankc", new IParser() {
            @Override
            public Message parse(byte[] content) throws InvalidProtocolBufferException {
                return Font.GlyphBank.parseFrom(content);
            }
        });
        parseMap.put("guic", new IParser() {
            @Override
            public Message parse(byte[] content) throws InvalidProtocolBufferException {
                return Gui.SceneDesc.parseFrom(content);
            }
        });
        parseMap.put("factory", new IParser() {
            @Override
            public Message parse(byte[] content) throws InvalidProtocolBufferException {
                FactoryDesc.Builder builder = FactoryDesc.newBuilder();
                try {
                    TextFormat.merge(new String(content), builder);
                } catch (ParseException e) {
                    throw new RuntimeException(e);
                }
                return builder.build();
            }
        });
        parseMap.put("materialc", new IParser() {
            @Override
            public Message parse(byte[] content) throws InvalidProtocolBufferException {
                return Material.MaterialDesc.parseFrom(content);
            }
        });
        parseMap.put("computec", new IParser() {
            @Override
            public Message parse(byte[] content) throws InvalidProtocolBufferException {
                return Compute.ComputeDesc.parseFrom(content);
            }
        });
        parseMap.put("renderc", new IParser() {
            @Override
            public Message parse(byte[] content) throws InvalidProtocolBufferException {
                return RenderPrototypeDesc.parseFrom(content);
            }
        });
        parseMap.put("factoryc", new IParser() {
            @Override
            public Message parse(byte[] content) throws InvalidProtocolBufferException {
                return FactoryDesc.parseFrom(content);
            }
        });
        parseMap.put("collectionfactory", new IParser() {
            @Override
            public Message parse(byte[] content) throws InvalidProtocolBufferException {
                CollectionFactoryDesc.Builder builder = CollectionFactoryDesc.newBuilder();
                try {
                    TextFormat.merge(new String(content), builder);
                } catch (ParseException e) {
                    throw new RuntimeException(e);
                }
                return builder.build();
            }
        });
        parseMap.put("collectionfactoryc", new IParser() {
            @Override
            public Message parse(byte[] content) throws InvalidProtocolBufferException {
                return CollectionFactoryDesc.parseFrom(content);
            }
        });
        parseMap.put("compcount_col", new IParser() {
            @Override
            public Message parse(byte[] content) throws InvalidProtocolBufferException {
                return null;
            }
        });
        parseMap.put("compcount_go", new IParser() {
            @Override
            public Message parse(byte[] content) throws InvalidProtocolBufferException {
                return null;
            }
        });
    }

    public static void addParser(String extension, IParser parser) {
        parseMap.put(extension, parser);
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
