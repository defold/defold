package com.dynamo.bob.pipeline;

import static org.junit.Assert.assertTrue;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.vecmath.AxisAngle4d;
import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;

import org.apache.commons.lang.StringEscapeUtils;
import org.junit.Assert;
import org.junit.Test;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.test.util.PropertiesTestUtil;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.gameobject.proto.GameObject.CollectionDesc;
import com.dynamo.gameobject.proto.GameObject.ComponentPropertyDesc;
import com.dynamo.gameobject.proto.GameObject.InstanceDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.dynamo.properties.proto.PropertiesProto.PropertyDeclarations;
import com.dynamo.proto.DdfMath.Point3;
import com.dynamo.proto.DdfMath.Quat;
import com.dynamo.sprite.proto.Sprite.SpriteDesc;
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
        CollectionDesc collection = (CollectionDesc)build("/test.collection", src.toString()).get(0);
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
        CollectionDesc collection = (CollectionDesc)build("/test.collection", src.toString()).get(0);

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
        Assert.assertEquals(0.5, inst.getScale3().getZ(), epsilon);

        inst = instances.get("/sub/sub_sub/test");
        assertEquals(new Point3d(0.5, 0, -0.5), inst.getPosition(), epsilon);
        assertEquals(new Quat4d(0, sq2, 0, -sq2), inst.getRotation(), epsilon);
        Assert.assertEquals(0.125, inst.getScale3().getX(), epsilon);
        Assert.assertEquals(0.125, inst.getScale3().getY(), epsilon);
        Assert.assertEquals(0.5, inst.getScale3().getZ(), epsilon);

        inst = instances.get("/sub/sub_sub/test_child");
        assertEquals(new Point3d(1, 0, 0), inst.getPosition(), epsilon);
        assertEquals(new Quat4d(0, sq2, 0, sq2), inst.getRotation(), epsilon);
        Assert.assertEquals(0.5, inst.getScale3().getX(), epsilon);
        Assert.assertEquals(0.5, inst.getScale3().getY(), epsilon);
        Assert.assertEquals(0.5, inst.getScale3().getZ(), epsilon);

        inst = instances.get("/sub/sub_sub/test_embed");
        assertEquals(new Point3d(0.5, 0, -0.5), inst.getPosition(), epsilon);
        assertEquals(new Quat4d(0, sq2, 0, -sq2), inst.getRotation(), epsilon);
        Assert.assertEquals(0.125, inst.getScale3().getX(), epsilon);
        Assert.assertEquals(0.125, inst.getScale3().getY(), epsilon);
        Assert.assertEquals(0.5, inst.getScale3().getZ(), epsilon);

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
        CollectionDesc collection = (CollectionDesc)build("/test.collection", src.toString()).get(0);

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
        Assert.assertEquals(0.5, inst.getScale3().getZ(), epsilon);
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
        Assert.assertEquals(1, messages.size());

        CollectionDesc collection = (CollectionDesc)messages.get(0);
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
        Assert.assertEquals(3, messages.size());

        CollectionDesc collection = (CollectionDesc)messages.get(0);
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
        Assert.assertEquals(7, messages.size());

        CollectionDesc collection = (CollectionDesc)messages.get(0);
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

        PrototypeDesc proto0 = (PrototypeDesc)messages.get(1);
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

        StringBuilder spriteSrc = new StringBuilder();
        spriteSrc.append("tile_set: \"/test.atlas\"\n");
        spriteSrc.append("default_animation: \"\"\n");

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

        CollectionDesc collection = (CollectionDesc)messages.get(0);
        Assert.assertEquals(1, collection.getInstancesCount());
        PrototypeDesc go = (PrototypeDesc)messages.get(2);
        Assert.assertEquals(1, go.getComponentsCount());
        SpriteDesc sprite = (SpriteDesc)messages.get(4);
    }
}
