// Copyright 2020 The Defold Foundation
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

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.util.Enumeration;
import java.util.List;
import java.util.Set;

import org.apache.commons.io.IOUtils;
import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.Path;
import org.junit.Test;

import static org.junit.Assert.assertTrue;

import com.dynamo.gamesys.proto.Gui;
import com.google.protobuf.Message;
import com.dynamo.bob.Project;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.ProtoParams;
import com.dynamo.bob.ProtoBuilder;
import com.dynamo.bob.ClassLoaderScanner;

public class GuiBuilderTest extends AbstractProtoBuilderTest {

    private ClassLoaderScanner scanner = null;


    public GuiBuilderTest() throws IOException {
        this.scanner = Project.createClassLoaderScanner();
        registerProtoBuilderNames();
    }

    private boolean nodeExists(Gui.SceneDesc scene, String nodeId)
    {
        for (int n = 0; n < scene.getNodesCount(); n++) {
            Gui.NodeDesc node = scene.getNodes(n);
            if (node.getId().equals(nodeId)) {
                return true;
            }
        }

        return false;
    }

    private void registerProtoBuilderNames() {
        Set<String> classNames = this.scanner.scan("com.dynamo.bob.pipeline");

        for (String className : classNames) {
            // Ignore TexcLibrary to avoid it being loaded and initialized
            boolean skip = className.startsWith("com.dynamo.bob.TexcLibrary");
            if (!skip) {
                try {
                    Class<?> klass = Class.forName(className, true, this.scanner.getClassLoader());
                    BuilderParams builderParams = klass.getAnnotation(BuilderParams.class);
                    if (builderParams != null) {
                        ProtoParams protoParams = klass.getAnnotation(ProtoParams.class);
                        if (protoParams != null) {
                            ProtoBuilder.addMessageClass(builderParams.outExt(), protoParams.messageClass());

                            for (String ext : builderParams.inExts()) {
                                Class<?> inputKlass = protoParams.srcClass();
                                if (inputKlass != null) {
                                    ProtoBuilder.addMessageClass(ext, protoParams.srcClass());
                                }
                            }
                        }
                    }

                } catch (Exception e) {
                    throw new RuntimeException(e);
                }
            }
        }
    }

    @Test
    public void test() throws Exception {
        // Kept empty as a future working template
    }
}
