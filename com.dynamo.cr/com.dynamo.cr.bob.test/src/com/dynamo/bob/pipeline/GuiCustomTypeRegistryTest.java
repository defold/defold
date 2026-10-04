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

import java.io.File;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Comparator;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.jar.JarEntry;
import java.util.jar.JarOutputStream;

import javax.tools.ToolProvider;

import org.junit.Assert;
import org.junit.Test;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Project;
import com.dynamo.gamesys.proto.Gui.NodeDesc;

public class GuiCustomTypeRegistryTest extends AbstractProtoBuilderTest {
    private static Path writePlugin(Path directory, boolean reject) throws Exception {
        Path node = directory.resolve("ReloadableNode.java");
        Files.writeString(node, """
                package gui.validation;
                import com.dynamo.bob.*;
                import com.dynamo.bob.fs.IResource;
                import com.dynamo.bob.pipeline.*;
                import com.dynamo.gamesys.proto.Gui.NodeDesc;
                import java.util.*;
                @GuiCustomNode(type = "Reloadable")
                public class ReloadableNode implements IGuiCustomNode {
                    public static void validateNodes(Project project, IResource resource,
                            List<NodeDesc> nodes, Map<String, IResource> resources) throws CompileExceptionError {
                        Helper.validate(project, resource);
                    }
                }
                """);
        Path helper = directory.resolve("Helper.java");
        Files.writeString(helper, """
                package gui.validation;
                import com.dynamo.bob.*;
                import com.dynamo.bob.fs.IResource;
                public class Helper {
                    public static void validate(Project project, IResource resource) throws CompileExceptionError {
                        project.setOption("reload-validation-calls",
                            Integer.toString(Integer.parseInt(project.option("reload-validation-calls", "0")) + 1));
                        %s
                    }
                }
                """.formatted(reject ? "throw new CompileExceptionError(resource, 0, \"Updated plugin validator rejected GUI\");" : ""));
        var compiler = ToolProvider.getSystemJavaCompiler();
        Assert.assertNotNull("The plugin reload test requires a JDK", compiler);
        var classpath = new LinkedHashSet<String>();
        for (Class<?> klass : List.of(Project.class, IGuiCustomNode.class, GuiCustomNode.class, NodeDesc.class)) {
            classpath.add(Path.of(klass.getProtectionDomain().getCodeSource().getLocation().toURI()).toString());
        }
        Assert.assertEquals(0, compiler.run(null, null, null, "-classpath", String.join(File.pathSeparator, classpath),
                "-d", directory.toString(), node.toString(), helper.toString()));
        Path jar = directory.resolve("plugin.jar");
        try (JarOutputStream output = new JarOutputStream(Files.newOutputStream(jar))) {
            for (String name : List.of("ReloadableNode", "Helper")) {
                String path = "gui/validation/" + name + ".class";
                output.putNextEntry(new JarEntry(path));
                Files.copy(directory.resolve(path), output);
                output.closeEntry();
            }
        }
        return jar;
    }

    private URLClassLoader loadPlugin(Path jar) throws Exception {
        URLClassLoader loader = new URLClassLoader(new URL[]{jar.toUri().toURL()}, getClass().getClassLoader());
        getProject().getGuiCustomTypeRegistry().register(loader.loadClass("gui.validation.ReloadableNode"));
        return loader;
    }

    // Revalidates unchanged GUI inputs when a plugin helper changes, while unchanged reloads stay cached.
    @Test
    public void testPluginHelperChangeInvalidatesGui() throws Exception {
        assertPluginHelperChangeInvalidatesGui(false);
    }

    // Tracks plugins used only by template nodes so their changed validators cannot leave a cached parent.
    @Test
    public void testPluginHelperChangeInvalidatesTemplateGui() throws Exception {
        assertPluginHelperChangeInvalidatesGui(true);
    }

    private void assertPluginHelperChangeInvalidatesGui(boolean throughTemplate) throws Exception {
        Path directory = Files.createTempDirectory("gui-validation-plugin-");
        try {
            Path jar = writePlugin(directory, false);
            byte[] nodeClass = Files.readAllBytes(directory.resolve("gui/validation/ReloadableNode.class"));
            String source = "material: \"\" nodes { id: \"node\" type: TYPE_CUSTOM custom_type_name: \"Reloadable\" }";
            if (throughTemplate) {
                addFile("/template.gui", source);
                source = "material: \"\" nodes { id: \"template\" type: TYPE_TEMPLATE template: \"/template.gui\" }";
            }
            try (URLClassLoader loader = loadPlugin(jar)) {
                build("/test.gui", source);
            }
            Assert.assertEquals("1", getProject().option("reload-validation-calls", "0"));
            try (URLClassLoader loader = loadPlugin(jar)) {
                build("/test.gui", source);
            }
            Assert.assertEquals("An unchanged plugin must preserve the cached GUI", "1", getProject().option("reload-validation-calls", "0"));

            writePlugin(directory, true);
            Assert.assertArrayEquals("Only the helper implementation changed", nodeClass,
                    Files.readAllBytes(directory.resolve("gui/validation/ReloadableNode.class")));
            try (URLClassLoader loader = loadPlugin(jar)) {
                try {
                    build("/test.gui", source);
                    Assert.fail("Expected the updated validator to reject the cached GUI");
                } catch (CompileExceptionError e) {
                    Assert.assertEquals("Updated plugin validator rejected GUI", e.getMessage());
                    Assert.assertEquals("test.gui", e.getResource().getPath());
                }
            }
            Assert.assertEquals("2", getProject().option("reload-validation-calls", "0"));
        } finally {
            try (var paths = Files.walk(directory)) {
                for (Path path : paths.sorted(Comparator.reverseOrder()).toList()) {
                    Files.delete(path);
                }
            }
        }
    }
}
