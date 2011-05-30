package com.dynamo.cr.protobind.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.junit.Test;

import com.dynamo.cr.protobind.IPath;
import com.dynamo.cr.protobind.MessageNode;
import com.dynamo.cr.protobind.RepeatedNode;
import com.dynamo.cr.protobind.proto.ProtoBind.ListMessage;
import com.dynamo.cr.protobind.proto.ProtoBind.NestedMessage;
import com.dynamo.cr.protobind.proto.ProtoBind.NestedNestedMessage;
import com.dynamo.cr.protobind.proto.ProtoBind.NodeDesc;
import com.dynamo.cr.protobind.proto.ProtoBind.NodeDesc.BlendMode;
import com.dynamo.cr.protobind.proto.ProtoBind.NodeDesc.Type;
import com.dynamo.cr.protobind.proto.ProtoBind.OptionalAndDefaultValueMessage;
import com.dynamo.cr.protobind.proto.ProtoBind.SceneDesc;
import com.dynamo.cr.protobind.proto.ProtoBind.SceneDesc.Builder;
import com.dynamo.cr.protobind.proto.ProtoBind.SimpleMessage;
import com.dynamo.proto.DdfMath.Vector4;

public class MessageNodeTest {

    static SimpleMessage createSimple() {
        SimpleMessage.Builder builder = SimpleMessage.newBuilder();
        builder.setA(1);
        builder.setB(123);
        builder.setS("foo");
        return builder.build();
    }

    private NestedMessage createNested() {
        NestedMessage.Builder builder = NestedMessage.newBuilder();
        return builder.setMsg(createSimple()).setX(88).build();
    }

    @Test
    public void testSetGetSimple() {
        SimpleMessage simpleMessage = createSimple();

        MessageNode simpleNode = new MessageNode(simpleMessage);

        IPath a = simpleNode.getPathTo("a");
        IPath b = simpleNode.getPathTo("b");
        IPath s = simpleNode.getPathTo("s");

        assertEquals(1, simpleNode.getField(a));
        assertEquals(123.0f, simpleNode.getField(b));
        assertEquals("foo", simpleNode.getField(s));

        assertEquals(1, simpleNode.getField("a"));
        assertEquals(123.0f, simpleNode.getField("b"));
        assertEquals("foo", simpleNode.getField("s"));

        simpleNode.setField(a, 10);
        simpleNode.setField(b, 1234.0f);
        simpleNode.setField(s, "foobar");

        assertEquals(10, simpleNode.getField(a));
        assertEquals(1234.0f, simpleNode.getField(b));
        assertEquals("foobar", simpleNode.getField(s));
    }

    @Test
    public void testAllPaths() {
        SimpleMessage simpleMessage = createSimple();

        MessageNode simpleNode = new MessageNode(simpleMessage);

        IPath[] paths = simpleNode.getAllPaths();
        Map<String, IPath> pathMap = new HashMap<String, IPath>();
        for (IPath p : paths) {
            pathMap.put(p.getName(), p);
        }

        assertEquals(3, pathMap.size());

        IPath a = pathMap.get("a");
        IPath b = pathMap.get("b");
        IPath s = pathMap.get("s");

        assertEquals(1, simpleNode.getField(a));
        assertEquals(123.0f, simpleNode.getField(b));
        assertEquals("foo", simpleNode.getField(s));

        simpleNode.setField(a, 10);
        simpleNode.setField(b, 1234.0f);
        simpleNode.setField(s, "foobar");

        assertEquals(10, simpleNode.getField(a));
        assertEquals(1234.0f, simpleNode.getField(b));
        assertEquals("foobar", simpleNode.getField(s));
    }

    @Test
    public void testNested() {
        NestedMessage nestedMessage = createNested();

        MessageNode nestedNode = new MessageNode(nestedMessage);

        IPath msg = nestedNode.getPathTo("msg");
        MessageNode simpleNode = (MessageNode) nestedNode.getField(msg);

        IPath a = simpleNode.getPathTo("a");
        IPath b = simpleNode.getPathTo("b");
        IPath s = simpleNode.getPathTo("s");

        assertEquals(1, nestedNode.getField(a));
        assertEquals(123.0f, nestedNode.getField(b));
        assertEquals("foo", nestedNode.getField(s));

        assertEquals(1, simpleNode.getField("a"));
        assertEquals(123.0f, simpleNode.getField("b"));
        assertEquals("foo", simpleNode.getField("s"));

        nestedNode.setField(a, 10);
        nestedNode.setField(b, 1234.0f);
        nestedNode.setField(s, "foobar");

        assertEquals(10, nestedNode.getField(a));
        assertEquals(1234.0f, nestedNode.getField(b));
        assertEquals("foobar", nestedNode.getField(s));

        NestedMessage nestedMessage2 = (NestedMessage) nestedNode.build();
        assertEquals(10, nestedMessage2.getMsg().getA());
        assertEquals(1234.0f, nestedMessage2.getMsg().getB(), 0.001f);
        assertEquals("foobar", nestedMessage2.getMsg().getS());
    }

    @Test
    public void testNestedNested() {
        NestedNestedMessage nestedNestedMessage = NestedNestedMessage.newBuilder().setNested(createNested()).build();

        MessageNode nestedNestedNode = new MessageNode(nestedNestedMessage);

        IPath nested = nestedNestedNode.getPathTo("nested");
        MessageNode nestedNode = (MessageNode) nestedNestedNode.getField(nested);

        IPath msg = nestedNode.getPathTo("msg");
        MessageNode simpleNode = (MessageNode) nestedNestedNode.getField(msg);

        IPath a = simpleNode.getPathTo("a");
        IPath b = simpleNode.getPathTo("b");
        IPath s = simpleNode.getPathTo("s");

        assertEquals(1, nestedNestedNode.getField(a));
        assertEquals(123.0f, nestedNestedNode.getField(b));
        assertEquals("foo", nestedNestedNode.getField(s));

        nestedNestedNode.setField(a, 10);
        nestedNestedNode.setField(b, 1234.0f);
        nestedNestedNode.setField(s, "foobar");

        assertEquals(10, nestedNestedNode.getField(a));
        assertEquals(1234.0f, nestedNestedNode.getField(b));
        assertEquals("foobar", nestedNestedNode.getField(s));

        NestedNestedMessage nestedMessage2 = (NestedNestedMessage) nestedNestedNode.build();
        assertEquals(10, nestedMessage2.getNested().getMsg().getA());
        assertEquals(1234.0f, nestedMessage2.getNested().getMsg().getB(), 0.001f);
        assertEquals("foobar", nestedMessage2.getNested().getMsg().getS());
    }

    @Test
    public void testListMessage() {
        ListMessage listMessage = ListMessage.newBuilder().addMessages(createSimple()).addMessages(createSimple()).build();

        MessageNode listMessageNode = new MessageNode(listMessage);
        IPath messages = listMessageNode.getPathTo("messages");

        RepeatedNode repeatedMessages = (RepeatedNode) listMessageNode.getField(messages);
        IPath elementOne = repeatedMessages.getPathTo(0);
        IPath elementTwo = repeatedMessages.getPathTo(1);
        List<SimpleMessage> messageList = repeatedMessages.getValueList();
        assertEquals(2, messageList.size());

        MessageNode one = (MessageNode) listMessageNode.getField(elementOne);
        MessageNode two = (MessageNode) listMessageNode.getField(elementTwo);

        assertEquals(1, listMessageNode.getField(one.getPathTo("a")));
        assertEquals(123.0f, listMessageNode.getField(one.getPathTo("b")));
        assertEquals("foo", listMessageNode.getField(one.getPathTo("s")));

        assertEquals(1, listMessageNode.getField(two.getPathTo("a")));
        assertEquals(123.0f, listMessageNode.getField(two.getPathTo("b")));
        assertEquals("foo", listMessageNode.getField(two.getPathTo("s")));

        listMessageNode.setField(one.getPathTo("a"), 10);
        listMessageNode.setField(one.getPathTo("b"), 1234.0f);
        listMessageNode.setField(one.getPathTo("s"), "foobar");

        listMessageNode.setField(elementTwo, SimpleMessage.newBuilder().setA(100).setB(12345.0f).setS("barfoo").build());

        assertEquals(10, listMessageNode.getField(one.getPathTo("a")));
        assertEquals(1234.0f, listMessageNode.getField(one.getPathTo("b")));
        assertEquals("foobar", listMessageNode.getField(one.getPathTo("s")));

        assertEquals(100, listMessageNode.getField(two.getPathTo("a")));
        assertEquals(12345.0f, listMessageNode.getField(two.getPathTo("b")));
        assertEquals("barfoo", listMessageNode.getField(two.getPathTo("s")));

        ListMessage listMessage2 = (ListMessage) listMessageNode.build();

        assertEquals(10, listMessage2.getMessages(0).getA());
        assertEquals(1234.0f, listMessage2.getMessages(0).getB(), 0.001f);
        assertEquals("foobar", listMessage2.getMessages(0).getS());

        assertEquals(100, listMessage2.getMessages(1).getA());
        assertEquals(12345.0f, listMessage2.getMessages(1).getB(), 0.001f);
        assertEquals("barfoo", listMessage2.getMessages(1).getS());
    }

    @Test
    public void testPath() {
        ListMessage listMessage = ListMessage.newBuilder().addMessages(createSimple()).addMessages(createSimple()).build();

        MessageNode listMessageNode = new MessageNode(listMessage);
        IPath messages = listMessageNode.getPathTo("messages");

        assertEquals(1, messages.elementCount());
        assertTrue(messages.lastElement().isField());
        assertFalse(messages.lastElement().isIndex());

        RepeatedNode repeatedMessages = (RepeatedNode) listMessageNode.getField(messages);
        IPath elementOne = repeatedMessages.getPathTo(0);
        assertEquals(2, elementOne.elementCount());
        assertFalse(elementOne.lastElement().isField());
        assertTrue(elementOne.lastElement().isIndex());

        IPath messages2 = elementOne.getParent();
        assertEquals(messages, messages2);
        assertEquals(messages.hashCode(), messages2.hashCode());

        RepeatedNode repeatedNode = (RepeatedNode) listMessageNode.getField(messages2);
        List<MessageNode> list = repeatedNode.getValueList();
        SimpleMessage msg = (SimpleMessage) list.get(0).build();
        assertEquals(1, msg.getA());
        assertEquals(123.0f, msg.getB(), 0.001f);
        assertEquals("foo", msg.getS());
    }

    @Test
    public void testListAddRemove() {
        ListMessage listMessage = ListMessage.newBuilder().build();

        MessageNode listMessageNode = new MessageNode(listMessage);
        IPath messages = listMessageNode.getPathTo("messages");

        RepeatedNode repeatedMessages = (RepeatedNode) listMessageNode.getField(messages);
        assertEquals(0, repeatedMessages.getValueList().size());

        listMessageNode.addRepeated(messages, createSimple());
        listMessageNode.addRepeated(messages, createSimple());
        listMessageNode.addRepeated(messages, SimpleMessage.newBuilder().setA(100).setB(12345.0f).setS("barfoo").build());

        assertEquals(3, repeatedMessages.getValueList().size());

        listMessageNode.removeRepeated(repeatedMessages.getPathTo(1));
        assertEquals(2, repeatedMessages.getValueList().size());

        RepeatedNode repeatedNode = (RepeatedNode) listMessageNode.getField(messages);
        List<MessageNode> list = repeatedNode.getValueList();

        SimpleMessage msg1 = (SimpleMessage) list.get(0).build();
        assertEquals(1, msg1.getA());
        assertEquals(123.0f, msg1.getB(), 0.001f);
        assertEquals("foo", msg1.getS());

        SimpleMessage msg2 = (SimpleMessage) list.get(1).build();
        assertEquals(100, msg2.getA());
        assertEquals(12345.0f, msg2.getB(), 0.001f);
        assertEquals("barfoo", msg2.getS());

        // Test that the paths gets updated when fields are removed
        // old paths are still invalid but new should be correct
        // We test that we don't get "2" instead of "1"
        assertEquals(100, listMessageNode.getField(list.get(1).getPathTo("a")));
        assertEquals(12345.0f, listMessageNode.getField(list.get(1).getPathTo("b")));
        assertEquals("barfoo", listMessageNode.getField(list.get(1).getPathTo("s")));
    }

    @Test
    public void testListMessageSetList() {
        ListMessage listMessage = ListMessage.newBuilder().build();

        MessageNode listMessageNode = new MessageNode(listMessage);
        IPath messages = listMessageNode.getPathTo("messages");
        listMessageNode.setField(messages, Arrays.asList(createSimple(), createSimple()));

        RepeatedNode repeatedMessages = (RepeatedNode) listMessageNode.getField(messages);
        IPath elementOne = repeatedMessages.getPathTo(0);
        IPath elementTwo = repeatedMessages.getPathTo(1);
        List<SimpleMessage> messageList = repeatedMessages.getValueList();
        assertEquals(2, messageList.size());

        MessageNode one = (MessageNode) listMessageNode.getField(elementOne);
        MessageNode two = (MessageNode) listMessageNode.getField(elementTwo);

        assertEquals(1, listMessageNode.getField(one.getPathTo("a")));
        assertEquals(123.0f, listMessageNode.getField(one.getPathTo("b")));
        assertEquals("foo", listMessageNode.getField(one.getPathTo("s")));

        assertEquals(1, listMessageNode.getField(two.getPathTo("a")));
        assertEquals(123.0f, listMessageNode.getField(two.getPathTo("b")));
        assertEquals("foo", listMessageNode.getField(two.getPathTo("s")));

        // Set repeated field with "self"
        listMessageNode.setField(messages, repeatedMessages.getValueList());

        assertEquals(1, listMessageNode.getField(one.getPathTo("a")));
        assertEquals(123.0f, listMessageNode.getField(one.getPathTo("b")));
        assertEquals("foo", listMessageNode.getField(one.getPathTo("s")));

        assertEquals(1, listMessageNode.getField(two.getPathTo("a")));
        assertEquals(123.0f, listMessageNode.getField(two.getPathTo("b")));
        assertEquals("foo", listMessageNode.getField(two.getPathTo("s")));
    }

    @Test
    public void testOptionalDefault() throws Exception {

        OptionalAndDefaultValueMessage msg1 = OptionalAndDefaultValueMessage.newBuilder().build();
        MessageNode node = new MessageNode(msg1);
        // Not set field are null
        assertEquals(0, node.getField("x"));
        assertEquals(123, node.getField("y"));
        assertEquals(SimpleMessage.newBuilder().setA(0).setB(0).setS("").build(), ((MessageNode) node.getField("msg")).build());

        OptionalAndDefaultValueMessage msg2 = (OptionalAndDefaultValueMessage) node.build();
        // Test that we don't "create" optional and not set fields
        assertEquals(msg1.hasX(), msg2.hasX());
        assertEquals(msg1.hasY(), msg2.hasY());
        // NOTE: Optional message are "created" unlike the behavior for scalar values
        // It's currently by design. It seems to be somewhat difficult to implement and the current
        // behavior seems sufficient in practice
        assertEquals(true, msg2.hasMsg());
    }

    @Test
    public void testGuiSceneBug() throws Exception {
        Builder sceneBuilder = SceneDesc.newBuilder();
        sceneBuilder.setReferenceWidth(100);
        sceneBuilder.setReferenceHeight(200);

        NodeDesc guiNode = NodeDesc.newBuilder()
            .setPosition(Vector4.newBuilder().setX(123))
            .setType(Type.TYPE_TEXT)
            .setBlendMode(BlendMode.BLEND_MODE_MULT).build();

        sceneBuilder.addNodes(guiNode);

        SceneDesc scene = sceneBuilder.build();

        MessageNode node = new MessageNode(scene);
        SceneDesc scene2 = (SceneDesc) node.build();
        assertEquals(scene.getReferenceWidth(), scene2.getReferenceWidth());
        assertEquals(scene.getReferenceHeight(), scene2.getReferenceHeight());
        assertEquals(scene.getNodesCount(), scene2.getNodesCount());
        assertEquals(123, scene2.getNodes(0).getPosition().getX(), 0.001);
    }

}
