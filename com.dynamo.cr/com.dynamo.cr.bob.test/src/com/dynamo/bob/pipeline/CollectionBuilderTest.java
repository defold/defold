package com.dynamo.bob.pipeline;

import org.junit.Test;

import com.dynamo.bob.Builder;
import com.dynamo.bob.pipeline.ProtoBuilders.CollectionBuilder;
import com.dynamo.bob.test.util.PropertiesTestUtil;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.gameobject.proto.GameObject.CollectionDesc;
import com.dynamo.gameobject.proto.GameObject.ComponentPropertyDesc;
import com.dynamo.gameobject.proto.GameObject.InstanceDesc;
import com.dynamo.properties.proto.PropertiesProto.PropertyDeclarations;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.Message;

public class CollectionBuilderTest extends AbstractProtoBuilderTest {

    @Override
    protected Builder<Void> createBuilder() {
        return new CollectionBuilder();
    }

    @Override
    protected Message parseMessage(byte[] content) throws InvalidProtocolBufferException {
        return CollectionDesc.parseFrom(content);
    }

    @Test
    public void testProps() throws Exception {
        addFile("/test.go", "");
        StringBuilder src = new StringBuilder();
        src.append("name: \"main\"\n");
        src.append("instances {\n");
        src.append("  id: \"test\"\n");
        src.append("  prototype: \"/test.go\"\n");
        src.append("  component_properties {\n");
        src.append("    id: \"test\"\n");
        src.append("    properties { id: \"number\" value: \"1\" type: PROPERTY_TYPE_NUMBER }\n");
        src.append("    properties { id: \"hash\" value: \"hash\" type: PROPERTY_TYPE_HASH }\n");
        src.append("    properties { id: \"url\" value: \"url\" type: PROPERTY_TYPE_URL }\n");
        src.append("    properties { id: \"vec3\" value: \"1, 2, 3\" type: PROPERTY_TYPE_VECTOR3 }\n");
        src.append("    properties { id: \"vec4\" value: \"4, 5, 6, 7\" type: PROPERTY_TYPE_VECTOR4 }\n");
        src.append("    properties { id: \"quat\" value: \"8, 9, 10, 11\" type: PROPERTY_TYPE_QUAT }\n");
        src.append("  }\n");
        src.append("}\n");
        CollectionDesc collection = (CollectionDesc)build("/test.collection", src.toString());
        for (InstanceDesc instance : collection.getInstancesList()) {
            for (ComponentPropertyDesc compProp : instance.getComponentPropertiesList()) {
                PropertyDeclarations properties = compProp.getPropertyDecls();
                PropertiesTestUtil.assertNumber(properties, 1, 0);
                PropertiesTestUtil.assertHash(properties, MurmurHash.hash64("hash"), 0);
                PropertiesTestUtil.assertURL(properties, "url", 0);
                PropertiesTestUtil.assertVector3(properties, 1, 2, 3, 0);
                PropertiesTestUtil.assertVector4(properties, 4, 5, 6, 7, 0);
                PropertiesTestUtil.assertQuat(properties, 8, 9, 10, 11, 0);
            }
        }
    }
}
