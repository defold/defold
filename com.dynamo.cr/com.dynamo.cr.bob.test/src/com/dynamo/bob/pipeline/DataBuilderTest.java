// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
+
+// You may obtain a copy of the License, together with FAQs at
+// https://www.defold.com/license
+
+// Unless required by applicable law or agreed to in writing, software distributed
+// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
+// CONDITIONS OF ANY KIND, either express or implied. See the License for the
+// specific language governing permissions and limitations under the License.
+
+package com.dynamo.bob.pipeline;
+
+import static org.junit.Assert.assertEquals;
+import static org.junit.Assert.assertTrue;
+
+import java.util.List;
+
+import org.junit.Test;
+
+import com.dynamo.gamesys.proto.DataProto.Data;
+import com.google.protobuf.Message;
+
+public class DataBuilderTest extends AbstractProtoBuilderTest {
+
+    @Test
+    public void testDataBuilder() throws Exception {
+        String source =
+                "tags: \"tag-one\"\n" +
+                "tags: \"tag-two\"\n" +
+                "data {\n" +
+                "  string: \"hello\"\n" +
+                "}\n";
+
+        List<Message> messages = build("/data/test.data", source);
+        assertEquals(1, messages.size());
+
+        Data data = getMessage(messages, Data.class);
+        assertTrue(data != null);
+
+        assertEquals(2, data.getTagsCount());
+        assertEquals("tag-one", data.getTags(0));
+        assertEquals("tag-two", data.getTags(1));
+
+        assertTrue(data.hasData());
+        assertEquals("hello", data.getData().getString());
+    }
+}

