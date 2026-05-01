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

import static org.junit.Assert.assertTrue;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.vecmath.AxisAngle4d;
import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;

import com.dynamo.gameobject.proto.GameObject;
import org.apache.commons.lang3.StringEscapeUtils;
import org.junit.Assert;
import org.junit.Test;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.test.util.PropertiesTestUtil;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.bob.util.ComponentsCounter;
import com.dynamo.gameobject.proto.GameObject.CollectionDesc;
import com.dynamo.gameobject.proto.GameObject.ComponentPropertyDesc;
import com.dynamo.gameobject.proto.GameObject.InstanceDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.dynamo.gameobject.proto.GameObject.ComponenTypeDesc;
import com.dynamo.properties.proto.PropertiesProto.PropertyDeclarations;
import com.dynamo.proto.DdfMath.Point3;
import com.dynamo.proto.DdfMath.Quat;
import com.dynamo.gamesys.proto.Sprite.SpriteDesc;
import com.dynamo.gamesys.proto.Sprite.SpriteTexture;
import com.google.protobuf.Message;

public class CollectionBuilderTest extends AbstractProtoBuilderTest {

    private static final double epsilon = 0.000001;

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
        src.append("    properties { id: \"bool\" value: \"true\" type: PROPERTY_TYPE_BOOLEAN }\n");
        src.append("  }\n");
        src.append("}\n");
        CollectionDesc collection = getMessage(build("/test.collection", src.toString()), CollectionDesc.class);
        for (InstanceDesc instance : collection.getInstancesList()) {
            for (ComponentPropertyDesc compProp : instance.getComponentPropertiesList()) {
                PropertyDeclarations properties = compProp.getPropertyDecls();
                PropertiesTestUtil.assertNumber(properties, 1, 0);
                PropertiesTestUtil.assertHash(properties, MurmurHash.hash64("hash"), 0);
                PropertiesTestUtil.assertURL(properties, "url", 0);
                PropertiesTestUtil.assertVector3(properties, 1, 2, 3, 0);
                PropertiesTestUtil.assertVector4(properties, 4, 5, 6, 7, 0);
                PropertiesTestUtil.assertQuat(properties, 8, 9, 10, 11, 0);
                PropertiesTestUtil.assertBoolean(properties, true, 0);
            }
        }
    }

    @Test(expected = CompileExceptionError.class)
    public void testPropInvalidValue() throws Exception {
        addFile("/test.go", "");
        StringBuilder src = new StringBuilder();
        src.append("name: \"main\"\n");
        src.append("instances {\n");
        src.append("  id: \"test\"\n");
        src.append("  prototype: \"/test.go\"\n");
        src.append("  component_properties {\n");
        src.append("    id: \"test\"\n");
        src.append("    properties { id: \"number\" value: \"a\" type: PROPERTY_TYPE_NUMBER }\n");
        src.append("  }\n");
        src.append("}\n");
        @SuppressWarnings("unused")
        CollectionDesc collection = (CollectionDesc)build("/test.collection", src.toString()).get(0);
    }

    private void addInstance(StringBuilder src, String id, String prototype, Point3d p, Quat4d r, double s, String ... childIds) {
        src.append("instances {\n");
        src.append("  id: \"").append(id).append("\"\n");
        src.append("  prototype: \"").append(prototype).append("\"\n");
        src.append("  position: { x: ").append(p.getX()).append(" y: ").append(p.getY()).append(" z: ").append(p.getZ()).append(" }\n");
        src.append("  rotation: { x: ").append(r.getX()).append(" y: ").append(r.getY()).append(" z: ").append(r.getZ()).append(" w: ").append(r.getW()).append(" }\n");
        src.append("  scale: ").append(s).append("\n");
        for (String childId : childIds) {
            src.append("  children: \"").append(childId).append("\"");
        }
        src.append("}\n");
    }

    private void addEmbeddedInstance(StringBuilder src, String id, Map<String, String> components, Point3d p, Quat4d r, double s, String ... childIds) {
        src.append("embedded_instances {\n");
        src.append("  id: \"").append(id).append("\"\n");
        StringBuilder dataBuilder = new StringBuilder();
        for (Map.Entry<String, String> component : components.entrySet()) {
            dataBuilder.append("components { id: \"").append(component.getKey()).append("\" component: \"").append(component.getValue()).append("\" }");
        }
        src.append("  data: \"").append(StringEscapeUtils.escapeJava(dataBuilder.toString())).append("\"\n");
        src.append("  position: { x: ").append(p.getX()).append(" y: ").append(p.getY()).append(" z: ").append(p.getZ()).append(" }\n");
        src.append("  rotation: { x: ").append(r.getX()).append(" y: ").append(r.getY()).append(" z: ").append(r.getZ()).append(" w: ").append(r.getW()).append(" }\n");
        src.append("  scale: ").append(s).append("\n");
        for (String childId : childIds) {
            src.append("  children: \"").append(childId).append("\"");
        }
        src.append("}\n");
    }

    private void addCollectionInstance(StringBuilder src, String id, String collection, Point3d p, Quat4d r, double s) {
        src.append("collection_instances {\n");
        src.append("  id: \"").append(id).append("\"\n");
        src.append("  collection: \"").append(collection).append("\"\n");
        src.append("  position: { x: ").append(p.getX()).append(" y: ").append(p.getY()).append(" z: ").append(p.getZ()).append(" }\n");
        src.append("  rotation: { x: ").append(r.getX()).append(" y: ").append(r.getY()).append(" z: ").append(r.getZ()).append(" w: ").append(r.getW()).append(" }\n");
        src.append("  scale: ").append(s).append("\n");
        src.append("}\n");
    }

    private static void assertEquals(Point3d v0, Point3 v1, double delta) {
        Assert.assertEquals(v0.getX(), v1.getX(), delta);
        Assert.assertEquals(v0.getY(), v1.getY(), delta);
        Assert.assertEquals(v0.getZ(), v1.getZ(), delta);
    }

    private static void assertEquals(Quat4d v0, Quat v1, double delta) {
        Assert.assertEquals(v0.getX(), v1.getX(), delta);
        Assert.assertEquals(v0.getY(), v1.getY(), delta);
        Assert.assertEquals(v0.getZ(), v1.getZ(), delta);
        Assert.assertEquals(v0.getW(), v1.getW(), delta);
    }

    /**
     * Test that a collection is flattened properly.
     * Structure:
     * - sub [collection]
     *   - sub_sub [collection]
     *     - test [instance]
     *       - test_child [instance]
     *     - test_embed [embedded instance]
     *       - test_embed_child [embedded instance]
     *   - test [instance]
     * - test [instance]
     * All instances have the local transform:
     * p = 1, 0, 0
     * r = 90 deg around Y
     * s = 0.5
     *
     * @throws Exception
     */
    @Test
    public void testCollectionFlattening() throws Exception {
        addFile("/test.go", "");

        Point3d p = new Point3d(1.0, 0.0, 0.0);
        Quat4d r = new Quat4d();
        r.set(new AxisAngle4d(new Vector3d(0, 1, 0), Math.PI * 0.5));
        double s = 0.5;

        StringBuilder subSubSrc = new StringBuilder();
        subSubSrc.append("name: \"sub_sub\"\n");
        addInstance(subSubSrc, "test_child", "/test.go", p, r, s);
        addInstance(subSubSrc, "test", "/test.go", p, r, s, "test_child");
        addEmbeddedInstance(subSubSrc, "test_embed_child", new HashMap<String, String>(), p, r, s);
        addEmbeddedInstance(subSubSrc, "test_embed", new HashMap<String, String>(), p, r, s, "test_embed_child");
        addFile("/sub_sub.collection", subSubSrc.toString());

        StringBuilder subSrc = new StringBuilder();
        subSrc.append("name: \"sub\"\n");
        addCollectionInstance(subSrc, "sub_sub", "/sub_sub.collection", p, r, s);
        addInstance(subSrc, "test", "/test.go", p, r, s);
        addFile("/sub.collection", subSrc.toString());

        StringBuilder src = new StringBuilder();
        src.append("name: \"main\"\n");
        addCollectionInstance(src, "sub", "/sub.collection", p, r, s);
        addInstance(src, "test", "/test.go", p, r, s);
        CollectionDesc collection = getMessage(build("/test.collection", src.toString()), CollectionDesc.class);

        Assert.assertEquals(6, collection.getInstancesCount());
        Assert.assertEquals(0, collection.getCollectionInstancesCount());

        Map<String, InstanceDesc> instances = new HashMap<String, InstanceDesc>();
        for (InstanceDesc inst : collection.getInstancesList()) {
            instances.put(inst.getId(), inst);
        }
        assertTrue(instances.containsKey("/test"));
        assertTrue(instances.containsKey("/sub/test"));
        assertTrue(instances.containsKey("/sub/sub_sub/test"));
        assertTrue(instances.containsKey("/sub/sub_sub/test_child"));
        assertTrue(instances.containsKey("/sub/sub_sub/test_embed"));
        assertTrue(instances.containsKey("/sub/sub_sub/test_embed_child"));

        InstanceDesc inst = instances.get("/test");
        assertEquals(new Point3d(1, 0, 0), inst.getPosition(), epsilon);
        double sq2 = Math.sqrt(2.0) * 0.5;
        assertEquals(new Quat4d(0, sq2, 0, sq2), inst.getRotation(), epsilon);
        Assert.assertEquals(0.5, inst.getScale3().getX(), epsilon);
        Assert.assertEquals(0.5, inst.getScale3().getY(), epsilon);
        Assert.assertEquals(0.5, inst.getScale3().getZ(), epsilon);

        inst = instances.get("/sub/test");
        assertEquals(new Point3d(1, 0, -0.5), inst.getPosition(), epsilon);
        assertEquals(new Quat4d(0, 1, 0, 0), inst.getRotation(), epsilon);
        Assert.assertEquals(0.25, inst.getScale3().getX(), epsilon);
        Assert.assertEquals(0.25, inst.getScale3().getY(), epsilon);
        Assert.assertEquals(0.25, inst.getScale3().getZ(), epsilon);

        inst = instances.get("/sub/sub_sub/test");
        assertEquals(new Point3d(0.75, 0, -0.5), inst.getPosition(), epsilon);
        assertEquals(new Quat4d(0, sq2, 0, -sq2), inst.getRotation(), epsilon);
        Assert.assertEquals(0.125, inst.getScale3().getX(), epsilon);
        Assert.assertEquals(0.125, inst.getScale3().getY(), epsilon);
        Assert.assertEquals(0.125, inst.getScale3().getZ(), epsilon);

        inst = instances.get("/sub/sub_sub/test_child");
        assertEquals(new Point3d(1, 0, 0), inst.getPosition(), epsilon);
        assertEquals(new Quat4d(0, sq2, 0, sq2), inst.getRotation(), epsilon);
        Assert.assertEquals(0.5, inst.getScale3().getX(), epsilon);
        Assert.assertEquals(0.5, inst.getScale3().getY(), epsilon);
        Assert.assertEquals(0.5, inst.getScale3().getZ(), epsilon);

        inst = instances.get("/sub/sub_sub/test_embed");
        assertEquals(new Point3d(0.75, 0, -0.5), inst.getPosition(), epsilon);
        assertEquals(new Quat4d(0, sq2, 0, -sq2), inst.getRotation(), epsilon);
        Assert.assertEquals(0.125, inst.getScale3().getX(), epsilon);
        Assert.assertEquals(0.125, inst.getScale3().getY(), epsilon);
        Assert.assertEquals(0.125, inst.getScale3().getZ(), epsilon);

        inst = instances.get("/sub/sub_sub/test_embed_child");
        assertEquals(new Point3d(1, 0, 0), inst.getPosition(), epsilon);
        assertEquals(new Quat4d(0, sq2, 0, sq2), inst.getRotation(), epsilon);
        Assert.assertEquals(0.5, inst.getScale3().getX(), epsilon);
        Assert.assertEquals(0.5, inst.getScale3().getY(), epsilon);
        Assert.assertEquals(0.5, inst.getScale3().getZ(), epsilon);

    }

    /**
     * Test that a collection is flattened properly w.r.t. children.
     * Structure:
     * - sub [collection]
     *   - child [instance]
     *   - parent [instance]
     * All instances have the local transform:
     * p = 1, 0, 0
     * r = 90 deg around Y
     * s = 0.5
     * Instances with no parent should have a concatenated transform, instances with a parent should retain their local transform.
     * @throws Exception
     */
    @Test
    public void testCollectionFlatteningChildren() throws Exception {
        addFile("/test.go", "");

        Point3d p = new Point3d(1.0, 0.0, 0.0);
        Quat4d r = new Quat4d();
        r.set(new AxisAngle4d(new Vector3d(0, 1, 0), Math.PI * 0.5));
        double s = 0.5;

        StringBuilder subSrc = new StringBuilder();
        subSrc.append("name: \"sub\"\n");
        addInstance(subSrc, "child", "/test.go", p, r, s);
        addInstance(subSrc, "parent", "/test.go", p, r, s, "child");
        addFile("/sub.collection", subSrc.toString());

        StringBuilder src = new StringBuilder();
        src.append("name: \"main\"\n");
        addCollectionInstance(src, "sub", "/sub.collection", p, r, s);
        CollectionDesc collection = getMessage(build("/test.collection", src.toString()), CollectionDesc.class);

        Assert.assertEquals(2, collection.getInstancesCount());
        Assert.assertEquals(0, collection.getCollectionInstancesCount());

        Map<String, InstanceDesc> instances = new HashMap<String, InstanceDesc>();
        for (InstanceDesc inst : collection.getInstancesList()) {
            instances.put(inst.getId(), inst);
        }
        assertTrue(instances.containsKey("/sub/parent"));
        assertTrue(instances.containsKey("/sub/child"));

        InstanceDesc inst = instances.get("/sub/parent");
        assertEquals(new Point3d(1, 0, -0.5), inst.getPosition(), epsilon);
        assertEquals(new Quat4d(0, 1, 0, 0), inst.getRotation(), epsilon);
        Assert.assertEquals(0.25, inst.getScale3().getX(), epsilon);
        Assert.assertEquals(0.25, inst.getScale3().getY(), epsilon);
        Assert.assertEquals(0.25, inst.getScale3().getZ(), epsilon);
        Assert.assertEquals("/sub/child", inst.getChildren(0));

        inst = instances.get("/sub/child");
        assertEquals(p, inst.getPosition(), epsilon);
        assertEquals(r, inst.getRotation(), epsilon);
        Assert.assertEquals(s, inst.getScale(), epsilon);
    }

    /**
     * Test that a collection is flattened properly w.r.t. properties.
     * Structure:
     * - sub [collection]
     *   - go [instance]
     * The sub instance overrides properties defined in sub.collection.
     * @throws Exception
     */
    @Test
    public void testCollectionFlatteningProperties() throws Exception {
        addFile("/test.go", "");
        StringBuilder src = new StringBuilder();
        src.append("name: \"sub\"\n");
        src.append("instances {\n");
        src.append("  id: \"go\"\n");
        src.append("  prototype: \"/test.go\"\n");
        src.append("  component_properties {\n");
        src.append("    id: \"test\"\n");
        src.append("    properties { id: \"number\" value: \"1\" type: PROPERTY_TYPE_NUMBER }\n");
        src.append("  }\n");
        src.append("}\n");
        addFile("/sub.collection", src.toString());

        src = new StringBuilder();
        src.append("name: \"main\"\n");
        src.append("collection_instances {\n");
        src.append("  id: \"sub\"\n");
        src.append("  collection: \"/sub.collection\"\n");
        src.append("  instance_properties: {\n");
        src.append("    id: \"go\"\n");
        src.append("    properties {\n");
        src.append("      id: \"test\"\n");
        src.append("      properties { id: \"number\" value: \"2\" type: PROPERTY_TYPE_NUMBER }\n");
        src.append("    }\n");
        src.append("  }\n");
        src.append("}\n");

        List<Message> messages = build("/test.collection", src.toString());
        Assert.assertEquals(3, messages.size());

        CollectionDesc collection = getMessage(messages, CollectionDesc.class);
        Assert.assertEquals(1, collection.getInstancesCount());
        Assert.assertEquals(0, collection.getCollectionInstancesCount());

        InstanceDesc instance = collection.getInstances(0);
        Assert.assertTrue(instance.getComponentProperties(0).getProperties(0).getValue().equals("2"));
    }

    /**
     * Test that a collection is flattened properly w.r.t. sub properties of embedded game objects.
     * Structure:
     * - sub [collection]
     *   - sub_sub [collection]
     *     - go [embedded instance]
     * The sub instance overrides properties defined in sub.collection.
     * @throws Exception
     */
    @Test
    public void testCollectionFlatteningSubProperties() throws Exception {
        StringBuilder src = new StringBuilder();
        src.append("name: \"sub_sub\"\n");
        src.append("embedded_instances {\n");
        src.append("  id: \"go\"\n");
        src.append("  data: \"\"\n");
        src.append("}\n");
        addFile("/sub_sub.collection", src.toString());

        src = new StringBuilder();
        src.append("name: \"sub\"\n");
        src.append("collection_instances {\n");
        src.append("  id: \"sub_sub\"\n");
        src.append("  collection: \"/sub_sub.collection\"\n");
        src.append("  instance_properties: {\n");
        src.append("    id: \"go\"\n");
        src.append("    properties {\n");
        src.append("      id: \"test\"\n");
        src.append("      properties { id: \"number\" value: \"2\" type: PROPERTY_TYPE_NUMBER }\n");
        src.append("    }\n");
        src.append("  }\n");
        src.append("}\n");
        addFile("/sub.collection", src.toString());

        src = new StringBuilder();
        src.append("name: \"main\"\n");
        src.append("collection_instances {\n");
        src.append("  id: \"sub\"\n");
        src.append("  collection: \"/sub.collection\"\n");
        src.append("}\n");

        List<Message> messages = build("/test.collection", src.toString());
        Assert.assertEquals(4, messages.size());

        CollectionDesc collection = getMessage(messages, CollectionDesc.class);
        Assert.assertEquals(1, collection.getInstancesCount());
        Assert.assertEquals(0, collection.getCollectionInstancesCount());

        InstanceDesc instance = collection.getInstances(0);
        Assert.assertTrue(instance.getComponentProperties(0).getProperties(0).getValue().equals("2"));
    }

    /**
     * Test that embedded instances are properly extracted.
     * Structure:
     * - sub [collection]
     *   - child [emb_instance]
     *   - parent [emb_instance]
     * go [emb_instance]
     * @throws Exception
     */
    @Test
    public void testEmbeddedInstances() throws Exception {
        Point3d p = new Point3d(1.0, 0.0, 0.0);
        Quat4d r = new Quat4d();
        r.set(new AxisAngle4d(new Vector3d(0, 1, 0), Math.PI * 0.5));
        double s = 0.5;

        Map<String, String> components = new HashMap<String, String>();

        StringBuilder subSrc = new StringBuilder();
        subSrc.append("name: \"sub\"\n");
        addEmbeddedInstance(subSrc, "child", components, p, r, s);
        addEmbeddedInstance(subSrc, "parent", components, p, r, s, "child");
        addFile("/sub.collection", subSrc.toString());

        StringBuilder src = new StringBuilder();
        src.append("name: \"main\"\n");
        addCollectionInstance(src, "sub", "/sub.collection", p, r, s);
        addEmbeddedInstance(src, "go", components, p, r, s);
        List<Message> messages = build("/test.collection", src.toString());
        Assert.assertEquals(3, messages.size()); // 7 original, but 3 when merged

        CollectionDesc collection = getMessage(messages, CollectionDesc.class);
        Assert.assertEquals(3, collection.getInstancesCount());
        Assert.assertEquals(0, collection.getCollectionInstancesCount());
        Assert.assertEquals(0, collection.getEmbeddedInstancesCount());

        Map<String, InstanceDesc> instances = new HashMap<String, InstanceDesc>();
        for (InstanceDesc inst : collection.getInstancesList()) {
            instances.put(inst.getId(), inst);
        }
        assertTrue(instances.containsKey("/sub/parent"));
        assertTrue(instances.containsKey("/sub/child"));
        assertTrue(instances.containsKey("/go"));

        PrototypeDesc proto0 = getMessage(messages, PrototypeDesc.class);
    }

    /**
     * Test that embedded instance with embedded components are properly built.
     * Structure:
     * - go [emb_instance]
     *   - sprite [emb_component]
     * @throws Exception
     */
    @Test
    public void testEmbeddedInstanceEmbeddedComponent() throws Exception {
        addFile("/test.atlas", "");
        addFile("build/test.a.texturesetc", "DUMMY_DATA");

        StringBuilder spriteSrc = new StringBuilder();
        spriteSrc.append("tile_set: \"/test.atlas\"\n");
        spriteSrc.append("default_animation: \"\"\n");
        spriteSrc.append("material: \"\"\n");

        StringBuilder goSrc = new StringBuilder();
        goSrc.append("embedded_components {\n");
        goSrc.append("  id: \"sprite\"\n");
        goSrc.append("  type: \"sprite\"\n");
        goSrc.append("  data: \"").append(StringEscapeUtils.escapeJava(spriteSrc.toString())).append("\"\n");
        goSrc.append("}\n");

        StringBuilder src = new StringBuilder();
        src.append("name: \"main\"\n");
        src.append("embedded_instances {\n");
        src.append("  id: \"go\"\n");
        src.append("  data: \"").append(StringEscapeUtils.escapeJava(goSrc.toString())).append("\"\n");
        src.append("}\n");

        List<Message> messages = build("/test.collection", src.toString());
        Assert.assertEquals(5, messages.size());

        CollectionDesc collection = getMessage(messages, CollectionDesc.class);
        Assert.assertEquals(1, collection.getInstancesCount());
        PrototypeDesc go = getMessage(messages, PrototypeDesc.class);
        Assert.assertEquals(1, go.getComponentsCount());
        SpriteDesc sprite = getMessage(messages, SpriteDesc.class);

        // Double check that it was removed..
        Assert.assertEquals(false, sprite.hasTileSet());
        // ...and replaced with a SpriteTexture
        Assert.assertEquals(1, sprite.getTexturesCount());
        SpriteTexture texture = sprite.getTextures(0);
        Assert.assertEquals("", texture.getSampler());
        Assert.assertEquals("/test.a.texturesetc", texture.getTexture());
    }

    /**
     * Test that component counter counts components right in embeded instances
     * Structure:
     * - go [emb_instance]
     *   - sprite [emb_component]
     *   - sprite [emb_component]
     * @throws Exception
     */
    @Test
    public void testEmbeddedInstanceComponentCounter() throws Exception {
        addFile("/test.atlas", "");
        addFile("build/test.a.texturesetc", "DUMMY_DATA");

        StringBuilder spriteSrc = new StringBuilder();
        spriteSrc.append("tile_set: \"/test.atlas\"\n");
        spriteSrc.append("default_animation: \"\"\n");
        spriteSrc.append("material: \"\"\n");

        StringBuilder goSrc = new StringBuilder();
        goSrc.append("embedded_components {\n");
        goSrc.append("  id: \"sprite\"\n");
        goSrc.append("  type: \"sprite\"\n");
        goSrc.append("  data: \"").append(StringEscapeUtils.escapeJava(spriteSrc.toString())).append("\"\n");
        goSrc.append("}\n");
        goSrc.append("embedded_components {\n");
        goSrc.append("  id: \"sprite\"\n");
        goSrc.append("  type: \"sprite\"\n");
        goSrc.append("  data: \"").append(StringEscapeUtils.escapeJava(spriteSrc.toString())).append("\"\n");
        goSrc.append("}\n");

        StringBuilder src = new StringBuilder();
        src.append("name: \"main\"\n");
        src.append("embedded_instances {\n");
        src.append("  id: \"go\"\n");
        src.append("  data: \"").append(StringEscapeUtils.escapeJava(goSrc.toString())).append("\"\n");
        src.append("}\n");

        List<Message> messages = build("/test.collection", src.toString());
        Assert.assertEquals(5, messages.size());

        CollectionDesc collection = getMessage(messages, CollectionDesc.class);
        List<ComponenTypeDesc> types = collection.getComponentTypesList();
        Assert.assertEquals(2, types.size());
        for (ComponenTypeDesc type: types) {
            if (type.getNameHash() == MurmurHash.hash64("spritec")) {
                Assert.assertEquals(2, type.getMaxCount());
            }
            else if (type.getNameHash() == MurmurHash.hash64("goc")) {
                Assert.assertEquals(1, type.getMaxCount());
            }
        }
    }

    /**
     * Test that the component counter counts components right in collection factory
     * Structure:
     * - go
     *   - collectionfactory
     *      - collection
     *          - go
     *              - sprite
     * @throws Exception
     */
    @Test
    public void testInstanceComponentCounterInCollectionFactory() throws Exception {
        addFile("/test.atlas", "");
        addFile("build/test.a.texturesetc", "DUMMY_DATA");

        StringBuilder spriteSrc = new StringBuilder();
        spriteSrc.append("tile_set: \"/test.atlas\"\n");
        spriteSrc.append("default_animation: \"\"\n");
        spriteSrc.append("material: \"\"\n");

        StringBuilder goTestSrc = new StringBuilder();
        goTestSrc.append("embedded_components {\n");
        goTestSrc.append("  id: \"sprite\"\n");
        goTestSrc.append("  type: \"sprite\"\n");
        goTestSrc.append("  data: \"").append(StringEscapeUtils.escapeJava(spriteSrc.toString())).append("\"\n");
        goTestSrc.append("}\n");

        StringBuilder srcTest = new StringBuilder();
        srcTest.append("name: \"factory_col\"\n");
        srcTest.append("embedded_instances {\n");
        srcTest.append("  id: \"go\"\n");
        srcTest.append("  data: \"").append(StringEscapeUtils.escapeJava(goTestSrc.toString())).append("\"\n");
        srcTest.append("}\n");

        addFile("/factory.collection", srcTest.toString());

        StringBuilder collectionfactorySrc = new StringBuilder();
        collectionfactorySrc.append("prototype: \"/factory.collection\"\n");
        collectionfactorySrc.append("\"\"\n");

        StringBuilder goSrc = new StringBuilder();
        goSrc.append("embedded_components {\n");
        goSrc.append("  id: \"collectionfactory\"\n");
        goSrc.append("  type: \"collectionfactory\"\n");
        goSrc.append("  data: \"").append(StringEscapeUtils.escapeJava(collectionfactorySrc.toString())).append("\"\n");
        goSrc.append("}\n");

        StringBuilder src = new StringBuilder();
        src.append("name: \"main\"\n");
        src.append("embedded_instances {\n");
        src.append("  id: \"go\"\n");
        src.append("  data: \"").append(StringEscapeUtils.escapeJava(goSrc.toString())).append("\"\n");
        src.append("}\n");

        List<Message> mainColmsg = build("/mainf.collection", src.toString());

        CollectionDesc collection = getMessage(mainColmsg, CollectionDesc.class);
        List<ComponenTypeDesc> types = collection.getComponentTypesList();
        Assert.assertEquals(3, types.size());
        for (ComponenTypeDesc type: types) {
            if (type.getNameHash() == MurmurHash.hash64("collectionfactoryc")) {
                Assert.assertEquals(1, type.getMaxCount());
            }
            else if (type.getNameHash() == MurmurHash.hash64("spritec")) {
                Assert.assertEquals((int)ComponentsCounter.DYNAMIC_VALUE, type.getMaxCount());
            }
            else if (type.getNameHash() == MurmurHash.hash64("goc")) {
                Assert.assertEquals((int)ComponentsCounter.DYNAMIC_VALUE, type.getMaxCount());
            }
        }
    }

    /**
     * Test that the component counter counts components right in factory
     * Structure:
     * - go
     *   - factory
     *     - go
     *       - sprite
     * @throws Exception
     */
    @Test
    public void testEmbededInstanceComponentCounterInFactory() throws Exception {
        addFile("/test.atlas", "");
        addFile("build/test.a.texturesetc", "DUMMY_DATA");

        StringBuilder spriteSrc = new StringBuilder();
        spriteSrc.append("tile_set: \"/test.atlas\"\n");
        spriteSrc.append("default_animation: \"\"\n");
        spriteSrc.append("material: \"\"\n");

        StringBuilder goTestSrc = new StringBuilder();
        goTestSrc.append("embedded_components {\n");
        goTestSrc.append("  id: \"sprite\"\n");
        goTestSrc.append("  type: \"sprite\"\n");
        goTestSrc.append("  data: \"").append(StringEscapeUtils.escapeJava(spriteSrc.toString())).append("\"\n");
        goTestSrc.append("}\n");

        addFile("/go.go", goTestSrc.toString());

        StringBuilder goFactorySrc = new StringBuilder();
        goFactorySrc.append("prototype: \"/go.go\"\n");
        goFactorySrc.append("\"\"\n");

        StringBuilder goSrc = new StringBuilder();
        goSrc.append("embedded_components {\n");
        goSrc.append("  id: \"factory\"\n");
        goSrc.append("  type: \"factory\"\n");
        goSrc.append("  data: \"").append(StringEscapeUtils.escapeJava(goFactorySrc.toString())).append("\"\n");
        goSrc.append("}\n");

        StringBuilder src = new StringBuilder();
        src.append("name: \"main\"\n");
        src.append("embedded_instances {\n");
        src.append("  id: \"go\"\n");
        src.append("  data: \"").append(StringEscapeUtils.escapeJava(goSrc.toString())).append("\"\n");
        src.append("}\n");

        List<Message> mainColmsg = build("/mainf.collection", src.toString());

        CollectionDesc collection = getMessage(mainColmsg, CollectionDesc.class);
        List<ComponenTypeDesc> types = collection.getComponentTypesList();
        Assert.assertEquals(3, types.size());
        for (ComponenTypeDesc type: types) {
            if (type.getNameHash() == MurmurHash.hash64("factoryc")) {
                Assert.assertEquals(1, type.getMaxCount());
            }
            else if (type.getNameHash() == MurmurHash.hash64("spritec")) {
                Assert.assertEquals((int)ComponentsCounter.DYNAMIC_VALUE, type.getMaxCount());
            }
            else if (type.getNameHash() == MurmurHash.hash64("goc")) {
                Assert.assertEquals((int)ComponentsCounter.DYNAMIC_VALUE, type.getMaxCount());
            }
        }
    }

    /**
     * Test that the component counter counts components right in factory
     * Structure:
     * - go [Instance tets1.go]
     *   - factory
     *      - collection
     *          - sprite
    * - go [Instance tets1.go]
     *   - factory
     *      - collection
     *          - sprite
     * @throws Exception
     */
    @Test
    public void testInstanceComponentCounterInFactory() throws Exception {
        addFile("/test.atlas", "");
        addFile("build/test.a.texturesetc", "DUMMY_DATA");

        StringBuilder spriteSrc = new StringBuilder();
        spriteSrc.append("tile_set: \"/test.atlas\"\n");
        spriteSrc.append("default_animation: \"\"\n");
        spriteSrc.append("material: \"\"\n");

        StringBuilder goTestSrc = new StringBuilder();
        goTestSrc.append("embedded_components {\n");
        goTestSrc.append("  id: \"sprite\"\n");
        goTestSrc.append("  type: \"sprite\"\n");
        goTestSrc.append("  data: \"").append(StringEscapeUtils.escapeJava(spriteSrc.toString())).append("\"\n");
        goTestSrc.append("}\n");

        addFile("/go.go", goTestSrc.toString());

        StringBuilder goFactorySrc = new StringBuilder();
        goFactorySrc.append("prototype: \"/go.go\"\n");
        goFactorySrc.append("\"\"\n");

        StringBuilder goSrc = new StringBuilder();
        goSrc.append("embedded_components {\n");
        goSrc.append("  id: \"factory\"\n");
        goSrc.append("  type: \"factory\"\n");
        goSrc.append("  data: \"").append(StringEscapeUtils.escapeJava(goFactorySrc.toString())).append("\"\n");
        goSrc.append("}\n");

        addFile("/test1.go", goSrc.toString());
        ComponentsCounter.Storage compStorage = ComponentsCounter.createStorage();
        compStorage.add("factoryc", 1);
        compStorage.add("sprite", ComponentsCounter.DYNAMIC_VALUE);

        StringBuilder src = new StringBuilder();
        src.append("name: \"main\"\n");
        src.append("instances {\n");
        src.append("  id: \"go\"\n");
        src.append("  prototype: \"/test1.go\"\n");
        src.append("}\n");
        src.append("instances {\n");
        src.append("  id: \"go\"\n");
        src.append("  prototype: \"/test1.go\"\n");
        src.append("}\n");

        List<Message> mainColmsg = build("/mainf.collection", src.toString());

        CollectionDesc collection = getMessage(mainColmsg, CollectionDesc.class);
        List<ComponenTypeDesc> types = collection.getComponentTypesList();
        Assert.assertEquals(3, types.size());
        for (ComponenTypeDesc type: types) {
            if (type.getNameHash() == MurmurHash.hash64("factoryc")) {
                Assert.assertEquals(2, type.getMaxCount());
            }
            else if (type.getNameHash() == MurmurHash.hash64("spritec")) {
                Assert.assertEquals((int)ComponentsCounter.DYNAMIC_VALUE, type.getMaxCount());
            }
            else if (type.getNameHash() == MurmurHash.hash64("goc")) {
                Assert.assertEquals((int)ComponentsCounter.DYNAMIC_VALUE, type.getMaxCount());
            }
        }
    }

    /**
     * Test that the component counter ignores collections with dynamic factories
     * Structure:
     * - go
     *   - factory
     *      - collection
     *          - sprite
     * @throws Exception
     */
    @Test
    public void testInstanceComponentCounterForDynamicFactory() throws Exception {
        addFile("/test.atlas", "");
        addFile("build/test.a.texturesetc", "DUMMY_DATA");

        StringBuilder spriteSrc = new StringBuilder();
        spriteSrc.append("tile_set: \"/test.atlas\"\n");
        spriteSrc.append("default_animation: \"\"\n");
        spriteSrc.append("material: \"\"\n");

        StringBuilder goTestSrc = new StringBuilder();
        goTestSrc.append("embedded_components {\n");
        goTestSrc.append("  id: \"sprite\"\n");
        goTestSrc.append("  type: \"sprite\"\n");
        goTestSrc.append("  data: \"").append(StringEscapeUtils.escapeJava(spriteSrc.toString())).append("\"\n");
        goTestSrc.append("}\n");

        addFile("/go.go", goTestSrc.toString());

        StringBuilder goFactorySrc = new StringBuilder();
        goFactorySrc.append("prototype: \"/go.go\"\n");
        goFactorySrc.append("\"\"\n");

        StringBuilder goSrc = new StringBuilder();
        goSrc.append("embedded_components {\n");
        goSrc.append("  id: \"factory\"\n");
        goSrc.append("  type: \"factory\"\n");
        goSrc.append("  data: \"").append(StringEscapeUtils.escapeJava(goFactorySrc.toString())).append("\"\n");
        goSrc.append("\"dynamic_prototype: true\"\n");
        goSrc.append("}\n");

        StringBuilder src = new StringBuilder();
        src.append("name: \"main\"\n");
        src.append("embedded_instances {\n");
        src.append("  id: \"go\"\n");
        src.append("  data: \"").append(StringEscapeUtils.escapeJava(goSrc.toString())).append("\"\n");
        src.append("}\n");

        List<Message> mainColmsg = build("/mainf.collection", src.toString());

        CollectionDesc collection = getMessage(mainColmsg, CollectionDesc.class);
        List<ComponenTypeDesc> types = collection.getComponentTypesList();
        Assert.assertEquals(0, types.size());
    }

    /**
     * Test that the component counter counts components in subcollection from collectionfactory
     * Structure:
     * - go
     *   - collectionfactory
     *      - collection
     *          - collection
     *              - go
     *                  - sprite
     * @throws Exception
     */
    @Test
    public void testInstanceComponentCounterInSubCollectionFromFactory() throws Exception {
        addFile("/test.atlas", "");
        addFile("build/test.a.texturesetc", "DUMMY_DATA");

        StringBuilder spriteSrc = new StringBuilder();
        spriteSrc.append("tile_set: \"/test.atlas\"\n");
        spriteSrc.append("default_animation: \"\"\n");
        spriteSrc.append("material: \"\"\n");

        StringBuilder goTestSrc = new StringBuilder();
        goTestSrc.append("embedded_components {\n");
        goTestSrc.append("  id: \"sprite\"\n");
        goTestSrc.append("  type: \"sprite\"\n");
        goTestSrc.append("  data: \"").append(StringEscapeUtils.escapeJava(spriteSrc.toString())).append("\"\n");
        goTestSrc.append("}\n");

        StringBuilder subCol = new StringBuilder();
        subCol.append("name: \"factory_sub_col\"\n");
        subCol.append("embedded_instances {\n");
        subCol.append("  id: \"go\"\n");
        subCol.append("  data: \"").append(StringEscapeUtils.escapeJava(goTestSrc.toString())).append("\"\n");
        subCol.append("}\n");

        addFile("/subCol.collection", subCol.toString());

        Point3d p = new Point3d(1.0, 0.0, 0.0);
        Quat4d r = new Quat4d();
        r.set(new AxisAngle4d(new Vector3d(0, 1, 0), Math.PI * 0.5));
        double s = 0.5;

        StringBuilder col = new StringBuilder();
        col.append("name: \"factory_col\"\n");
        addCollectionInstance(col, "subCol", "/subCol.collection", p, r, s);

        addFile("/factory.collection", col.toString());

        StringBuilder collectionfactorySrc = new StringBuilder();
        collectionfactorySrc.append("prototype: \"/factory.collection\"\n");
        collectionfactorySrc.append("\"\"\n");

        StringBuilder goSrc = new StringBuilder();
        goSrc.append("embedded_components {\n");
        goSrc.append("  id: \"collectionfactory\"\n");
        goSrc.append("  type: \"collectionfactory\"\n");
        goSrc.append("  data: \"").append(StringEscapeUtils.escapeJava(collectionfactorySrc.toString())).append("\"\n");
        goSrc.append("}\n");

        StringBuilder src = new StringBuilder();
        src.append("name: \"main\"\n");
        src.append("embedded_instances {\n");
        src.append("  id: \"go\"\n");
        src.append("  data: \"").append(StringEscapeUtils.escapeJava(goSrc.toString())).append("\"\n");
        src.append("}\n");

        List<Message> mainColmsg = build("/mainf.collection", src.toString());

        CollectionDesc collection = getMessage(mainColmsg, CollectionDesc.class);
        List<ComponenTypeDesc> types = collection.getComponentTypesList();
        Assert.assertEquals(3, types.size());
        for (ComponenTypeDesc type: types) {
            if (type.getNameHash() == MurmurHash.hash64("collectionfactoryc")) {
                Assert.assertEquals(1, type.getMaxCount());
            }
            else if (type.getNameHash() == MurmurHash.hash64("spritec")) {
                Assert.assertEquals((int)ComponentsCounter.DYNAMIC_VALUE, type.getMaxCount());
            }
            else if (type.getNameHash() == MurmurHash.hash64("goc")) {
                Assert.assertEquals((int)ComponentsCounter.DYNAMIC_VALUE, type.getMaxCount());
            }
        }
    }

    /**
     * Test component counter for sub collections
     * Structure:
     * - subCol [collection]
     *   - child [emb_instance]
     *     - sprite
     * - subCol2 [collection]
     *   - child [emb_instance]
     *     - sprite
     * @throws Exception
     */
    @Test
    public void testSubCollectionCounter() throws Exception {
        Point3d p = new Point3d(1.0, 0.0, 0.0);
        Quat4d r = new Quat4d();
        r.set(new AxisAngle4d(new Vector3d(0, 1, 0), Math.PI * 0.5));
        double s = 0.5;

        addFile("/test.atlas", "");
        addFile("build/test.a.texturesetc", "DUMMY_DATA");

        StringBuilder spriteSrc = new StringBuilder();
        spriteSrc.append("tile_set: \"/test.atlas\"\n");
        spriteSrc.append("default_animation: \"\"\n");
        spriteSrc.append("material: \"\"\n");

        StringBuilder goTestSrc = new StringBuilder();
        goTestSrc.append("embedded_components {\n");
        goTestSrc.append("  id: \"sprite\"\n");
        goTestSrc.append("  type: \"sprite\"\n");
        goTestSrc.append("  data: \"").append(StringEscapeUtils.escapeJava(spriteSrc.toString())).append("\"\n");
        goTestSrc.append("}\n");

        StringBuilder subCol = new StringBuilder();
        subCol.append("name: \"sub_col\"\n");
        subCol.append("embedded_instances {\n");
        subCol.append("  id: \"go\"\n");
        subCol.append("  data: \"").append(StringEscapeUtils.escapeJava(goTestSrc.toString())).append("\"\n");
        subCol.append("}\n");

        addFile("/subCol.collection", subCol.toString());

        StringBuilder col = new StringBuilder();
        col.append("name: \"test_col\"\n");
        addCollectionInstance(col, "subCol", "/subCol.collection", p, r, s);
        addCollectionInstance(col, "subCol2", "/subCol.collection", p, r, s);

        List<Message> messages = build("/test.collection", col.toString());

        Assert.assertEquals(6, messages.size());

        CollectionDesc collection = getMessage(messages, CollectionDesc.class);
        Assert.assertEquals(2, collection.getInstancesCount());
        Assert.assertEquals(0, collection.getCollectionInstancesCount());
        Assert.assertEquals(0, collection.getEmbeddedInstancesCount());

        Map<String, InstanceDesc> instances = new HashMap<String, InstanceDesc>();
        for (InstanceDesc inst : collection.getInstancesList()) {
            instances.put(inst.getId(), inst);
        }
        assertTrue(instances.containsKey("/subCol/go"));
        assertTrue(instances.containsKey("/subCol2/go"));

        List<ComponenTypeDesc> types = collection.getComponentTypesList();
        Assert.assertEquals(2, types.size());
        for (ComponenTypeDesc type: types) {
            if (type.getNameHash() == MurmurHash.hash64("spritec")) {
                Assert.assertEquals(2, type.getMaxCount());
            }
            else if (type.getNameHash() == MurmurHash.hash64("goc")) {
                Assert.assertEquals(2, type.getMaxCount());
            }
        }
    }

    /**
     * Test that the objects in the collection sorted according to its transform hierarhy
     * Structure:
     * - go "0"
     *   - go "1"
     *      - go "2"
     *          - go "3"
     * @throws Exception
     */
    @Test
    public void testEmbeddedInstancesOrder() throws Exception {
        StringBuilder src = new StringBuilder();
        src.append("name: \"Example\"\n");
        src.append("scale_along_z: 0\n");
        src.append("embedded_instances {\n");
        src.append("  id: \"3\"\n"); // Create the last object in hierarhy the first in the source file
        src.append("  data: \"\"\n");
        src.append("}\n");
        src.append("embedded_instances {\n");
        src.append("  id: \"0\"\n");
        src.append("  children: \"1\"\n");
        src.append("  data: \"\"\n");
        src.append("}\n");
        src.append("embedded_instances {\n");
        src.append("  id: \"1\"\n");
        src.append("  children: \"2\"\n");
        src.append("  data: \"\"\n");
        src.append("}\n");
        src.append("embedded_instances {\n");
        src.append("  id: \"2\"\n");
        src.append("  children: \"3\"\n");
        src.append("  data: \"\"\n");
        src.append("}\n");

        CollectionDesc collection = getMessage(build("/test.collection", src.toString()), CollectionDesc.class);
        List<GameObject.InstanceDesc> instances = collection.getInstancesList();

        Assert.assertEquals("Order of instances should be 0, 1, 2, 3", 4, instances.size());
        Assert.assertEquals("First instance ID should be '0'", "/0", instances.get(0).getId());
        Assert.assertEquals("Second instance ID should be '1'", "/1", instances.get(1).getId());
        Assert.assertEquals("Third instance ID should be '2'", "/2", instances.get(2).getId());
        Assert.assertEquals("Fourth instance ID should be '3'", "/3", instances.get(3).getId());
    }
}
