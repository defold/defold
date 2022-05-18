// Copyright 2020-2022 The Defold Foundation
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

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.RandomAccessFile;
import java.util.Collection;
import java.util.HashSet;
import java.util.Set;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.net.URLClassLoader;
import java.net.URL;
import java.lang.Math;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector4d;

import org.apache.commons.io.IOUtils;

import com.dynamo.bob.Bob;
import com.dynamo.bob.Builder;
import com.dynamo.bob.Project;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Task;
import com.dynamo.bob.IClassScanner;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.LuaScanner.Property.Status;
import com.dynamo.bob.plugin.PluginScanner;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.bob.util.PropertiesUtil;
import com.dynamo.gameobject.proto.GameObject.PropertyType;
import com.dynamo.lua.proto.Lua.LuaModule;
import com.dynamo.properties.proto.PropertiesProto.PropertyDeclarationEntry;
import com.dynamo.properties.proto.PropertiesProto.PropertyDeclarations;
import com.dynamo.script.proto.Lua.LuaSource;
import com.google.protobuf.ByteString;
import com.google.protobuf.Message;

/**
 * Lua builder. This class is abstract. Inherit from this class
 * and add appropriate {@link BuilderParams}
 * @author chmu
 *
 */
public abstract class LuaBuilder extends Builder<Void> {

    private static ArrayList<Platform> needsLuaSource = new ArrayList<Platform>(Arrays.asList(Platform.JsWeb, Platform.WasmWeb));

    private static LuaBuilderPlugin luaBuilderPlugin = null;


    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
        Task.TaskBuilder<Void> taskBuilder = Task.<Void>newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(params.outExt()));

        String script = new String((input.getContent()));
        LuaScanner scanner = new LuaScanner();
        scanner.parse(script);

        List<LuaScanner.Property> properties = scanner.getProperties();
        for (LuaScanner.Property property : properties) {
            if (property.type == PropertyType.PROPERTY_TYPE_HASH) {
                String value = (String)property.value;
                if (PropertiesUtil.isResourceProperty(project, property.type, value)) {
                    IResource resource = BuilderUtil.checkResource(this.project, input, property.name + " resource", value);
                    taskBuilder.addInput(resource);
                    PropertiesUtil.createResourcePropertyTasks(this.project, resource, input);
                }
            }
        }

        return taskBuilder.build();
    }

    public byte[] constructBytecode(Task<Void> task, String luajitExe, String source) throws IOException, CompileExceptionError {

        java.io.FileOutputStream fo = null;
        RandomAccessFile rdr = null;

        try {
            Bob.initLua(); // unpack the lua resources

            File outputFile = File.createTempFile("script", ".raw");
            File inputFile = File.createTempFile("script", ".lua");

            // Need to write the input file separately in case it comes from built-in, and cannot
            // be found through its path alone.
            fo = new java.io.FileOutputStream(inputFile);
            fo.write(source.getBytes());
            fo.close();

            // Doing a bit of custom set up here as the path is required.
            //
            // NOTE: The -f option for bytecode is a small custom modification to bcsave.lua in LuaJIT which allows us to supply the
            //       correct chunk name (the original source file) already here.
            //
            // See implementation of luaO_chunkid and why a prefix '@' is used; it is to show the last 60 characters of the name.
            //
            // If a script error occurs in runtime we want Lua to report the end of the filepath
            // associated with the chunk, since this is where the filename is visible.
            //
            final String chunkName = "@" + task.input(0).getPath();
            List<String> options = new ArrayList<String>();
            options.add(Bob.getExe(Platform.getHostPlatform(), luajitExe));
            options.add("-b");
            options.add("-g");
            options.add("-f"); options.add(chunkName);
            options.add(inputFile.getAbsolutePath());
            options.add(outputFile.getAbsolutePath());


            ProcessBuilder pb = new ProcessBuilder(options).redirectErrorStream(true);

            java.util.Map<String, String> env = pb.environment();
            env.put("LUA_PATH", Bob.getPath("share/luajit/") + "/?.lua");

            Process p = pb.start();
            InputStream is = null;
            int ret = 127;

            try {
                ret = p.waitFor();
                is = p.getInputStream();

                int toRead = is.available();
                byte[] buf = new byte[toRead];
                is.read(buf);

                String cmdOutput = new String(buf);
                if (ret != 0) {
                    // first delimiter is the executable name "luajit:"
                    int execSep = cmdOutput.indexOf(':');
                    if (execSep > 0) {
                        // then comes the filename and the line like this:
                        // "file.lua:30: <error message>"
                        int lineBegin = cmdOutput.indexOf(':', execSep + 1);
                        if (lineBegin > 0) {
                            int lineEnd = cmdOutput.indexOf(':', lineBegin + 1);
                            if (lineEnd > 0) {
                                throw new CompileExceptionError(task.input(0),
                                        Integer.parseInt(cmdOutput.substring(
                                                lineBegin + 1, lineEnd)),
                                        cmdOutput.substring(lineEnd + 2));
                            }
                        }
                    }
                    // Since parsing out the actual error failed, as a backup just
                    // spit out whatever luajit said.
                    inputFile.delete();
                    throw new CompileExceptionError(task.input(0), 1, cmdOutput);
                }
            } catch (InterruptedException e) {
                Logger.getLogger(LuaBuilder.class.getCanonicalName()).log(Level.SEVERE, "Unexpected interruption", e);
            } finally {
                IOUtils.closeQuietly(is);
            }

            long resultBytes = outputFile.length();
            rdr = new RandomAccessFile(outputFile, "r");
            byte tmp[] = new byte[(int) resultBytes];
            rdr.readFully(tmp);

            outputFile.delete();
            inputFile.delete();
            return tmp;
        }
        finally {
            IOUtils.closeQuietly(fo);
            IOUtils.closeQuietly(rdr);
        }
    }

    @Override
    public void build(Task<Void> task) throws CompileExceptionError, IOException {

        LuaModule.Builder builder = LuaModule.newBuilder();
        final byte[] originalScriptBytes = task.input(0).getContent();
        final String originalScript = new String(originalScriptBytes, "UTF-8");

        // run the Lua scanner to get and remove require and properties
        LuaScanner scanner = new LuaScanner();
        String script = scanner.parse(originalScript);
        List<String> modules = scanner.getModules();
        List<LuaScanner.Property> properties = scanner.getProperties();

        // add detected modules to builder
        for (String module : modules) {
            String module_file = String.format("/%s.lua", module.replaceAll("\\.", "/"));
            BuilderUtil.checkResource(this.project, task.input(0), "module", module_file);
            builder.addModules(module);
            builder.addResources(module_file + "c");
        }

        // add detected properties to builder
        Collection<String> propertyResources = new HashSet<String>();
        PropertyDeclarations propertiesMsg = buildProperties(task.input(0), properties, propertyResources);
        builder.setProperties(propertiesMsg);
        builder.addAllPropertyResources(propertyResources);

        // create and apply a builder plugin if one exists
        LuaBuilderPlugin luaBuilderPlugin = PluginScanner.getOrCreatePlugin("com.dynamo.bob.pipeline", LuaBuilderPlugin.class);
        if (luaBuilderPlugin != null) {
            try {
                script = luaBuilderPlugin.build(script);
            }
            catch(Exception e) {
                throw new CompileExceptionError(task.input(0), 0, "Unable to run Lua builder plugin", e);
            }
        }

        // if for some reason, the project needs to be bundled with regular Lua files (e.g. on iOS)
        boolean use_vanilla_lua = this.project.option("use-vanilla-lua", "false").equals("true");
        if (use_vanilla_lua) {
            for(IResource res : task.getOutputs()) {
                String path = res.getAbsPath();
                if(path.endsWith("luac") || path.endsWith("scriptc") || path.endsWith("gui_scriptc") || path.endsWith("render_scriptc")) {
                    project.addOutputFlags(path, Project.OutputFlags.UNCOMPRESSED);
                }
            }
        }

        LuaSource.Builder srcBuilder = LuaSource.newBuilder();
        srcBuilder.setFilename(task.input(0).getPath());

        if (needsLuaSource.contains(project.getPlatform()) || use_vanilla_lua) {
            srcBuilder.setScript(ByteString.copyFrom(script.getBytes()));
        } else {
            byte[] bytecode32 = constructBytecode(task, "luajit-32", script);
            byte[] bytecode64 = constructBytecode(task, "luajit-64", script);

            if (bytecode32.length != bytecode64.length) {
                Bob.verbose("%s bytecode32.length != bytecode64.length", task.input(0).getPath());
                throw new CompileExceptionError(task.input(0), 0, "Byte code length mismatch");
            }

            srcBuilder.setBytecode(ByteString.copyFrom(bytecode64));

            ByteArrayOutputStream bos = new ByteArrayOutputStream();
            Bob.verbose("%s 32bit length: %d 64bit length: %d", task.input(0).getPath(), bytecode32.length, bytecode64.length);
            int previousDiffIndex = 0;
            int previousDiff = 0;
            int maxDiffStep = 0;
            int maxConsecutive = 0;
            int consecutive = 0;
            int diffCount = 0;
            for(int i = 0; i < bytecode32.length; i++) {
                byte b32 = bytecode32[i];
                byte b64 = bytecode64[i];
                if (b32 != b64) {
                    int diff = b64 - b32;
                    //Bob.verbose("i = %d b32 = %d b64 = %d diff = %d", i, b32, b64, b64-b32);
                    bos.write(i);
                    if (bytecode32.length > 255) {
                        bos.write((i & 0xFF00) >> 8);
                    }
                    if (bytecode32.length > 65535) {
                        bos.write((i & 0xFF0000) >> 16);
                        bos.write((i & 0xFF000000) >> 24);
                    }
                    bos.write(diff);

                    if (previousDiffIndex == (i - 1)) {
                        consecutive++;
                    }
                    else {
                        consecutive = 0;
                    }
                    maxDiffStep = Math.max(maxDiffStep, i - previousDiffIndex);
                    maxConsecutive = Math.max(maxConsecutive, consecutive);
                    previousDiffIndex = i;
                    previousDiff = diff;
                    diffCount++;
                }
            }
            byte[] delta = bos.toByteArray();
            Bob.verbose("%s delta size: %d diffCount: %d maxDiffStep: %d maxConsecutive: %d", task.input(0).getPath(), delta.length, diffCount, maxDiffStep, maxConsecutive);
            srcBuilder.setDelta(ByteString.copyFrom(delta));
        }

        builder.setSource(srcBuilder);

        Message msg = builder.build();
        ByteArrayOutputStream out = new ByteArrayOutputStream(4 * 1024);
        msg.writeTo(out);
        out.close();
        task.output(0).setContent(out.toByteArray());
    }

    private PropertyDeclarations buildProperties(IResource resource, List<LuaScanner.Property> properties, Collection<String> propertyResources) throws CompileExceptionError {
        PropertyDeclarations.Builder builder = PropertyDeclarations.newBuilder();
        if (!properties.isEmpty()) {
            for (LuaScanner.Property property : properties) {
                if (property.status == Status.OK) {
                    PropertyDeclarationEntry.Builder entryBuilder = PropertyDeclarationEntry.newBuilder();
                    entryBuilder.setKey(property.name);
                    entryBuilder.setId(MurmurHash.hash64(property.name));
                    switch (property.type) {
                    case PROPERTY_TYPE_NUMBER:
                        entryBuilder.setIndex(builder.getFloatValuesCount());
                        builder.addFloatValues(((Double)property.value).floatValue());
                        builder.addNumberEntries(entryBuilder);
                        break;
                    case PROPERTY_TYPE_HASH:
                        String value = (String)property.value;
                        if (PropertiesUtil.isResourceProperty(project, property.type, value)) {
                            value = PropertiesUtil.transformResourcePropertyValue(value);
                            propertyResources.add(value);
                        }
                        entryBuilder.setIndex(builder.getHashValuesCount());
                        builder.addHashValues(MurmurHash.hash64(value));
                        builder.addHashEntries(entryBuilder);
                        break;
                    case PROPERTY_TYPE_URL:
                        entryBuilder.setIndex(builder.getStringValuesCount());
                        builder.addStringValues((String)property.value);
                        builder.addUrlEntries(entryBuilder);
                        break;
                    case PROPERTY_TYPE_VECTOR3:
                        entryBuilder.setIndex(builder.getFloatValuesCount());
                        entryBuilder.addElementIds(MurmurHash.hash64(property.name + ".x"));
                        entryBuilder.addElementIds(MurmurHash.hash64(property.name + ".y"));
                        entryBuilder.addElementIds(MurmurHash.hash64(property.name + ".z"));
                        Vector3d v3 = (Vector3d)property.value;
                        builder.addFloatValues((float)v3.getX());
                        builder.addFloatValues((float)v3.getY());
                        builder.addFloatValues((float)v3.getZ());
                        builder.addVector3Entries(entryBuilder);
                        break;
                    case PROPERTY_TYPE_VECTOR4:
                        entryBuilder.setIndex(builder.getFloatValuesCount());
                        entryBuilder.addElementIds(MurmurHash.hash64(property.name + ".x"));
                        entryBuilder.addElementIds(MurmurHash.hash64(property.name + ".y"));
                        entryBuilder.addElementIds(MurmurHash.hash64(property.name + ".z"));
                        entryBuilder.addElementIds(MurmurHash.hash64(property.name + ".w"));
                        Vector4d v4 = (Vector4d)property.value;
                        builder.addFloatValues((float)v4.getX());
                        builder.addFloatValues((float)v4.getY());
                        builder.addFloatValues((float)v4.getZ());
                        builder.addFloatValues((float)v4.getW());
                        builder.addVector4Entries(entryBuilder);
                        break;
                    case PROPERTY_TYPE_QUAT:
                        entryBuilder.setIndex(builder.getFloatValuesCount());
                        entryBuilder.addElementIds(MurmurHash.hash64(property.name + ".x"));
                        entryBuilder.addElementIds(MurmurHash.hash64(property.name + ".y"));
                        entryBuilder.addElementIds(MurmurHash.hash64(property.name + ".z"));
                        entryBuilder.addElementIds(MurmurHash.hash64(property.name + ".w"));
                        Quat4d q = (Quat4d)property.value;
                        builder.addFloatValues((float)q.getX());
                        builder.addFloatValues((float)q.getY());
                        builder.addFloatValues((float)q.getZ());
                        builder.addFloatValues((float)q.getW());
                        builder.addQuatEntries(entryBuilder);
                        break;
                    case PROPERTY_TYPE_BOOLEAN:
                        entryBuilder.setIndex(builder.getFloatValuesCount());
                        builder.addFloatValues(((Boolean)property.value) ? 1.0f : 0.0f);
                        builder.addBoolEntries(entryBuilder);
                        break;
                    }
                } else if (property.status == Status.INVALID_ARGS) {
                    throw new CompileExceptionError(resource, property.line + 1, "go.property takes a string and a value as arguments. The value must have the type number, boolean, hash, msg.url, vmath.vector3, vmath.vector4, vmath.quat, or resource.*.");
                } else if (property.status == Status.INVALID_VALUE) {
                    throw new CompileExceptionError(resource, property.line + 1, "Only these types are available: number, hash, msg.url, vmath.vector3, vmath.vector4, vmath.quat, resource.*");
                }
            }
        }
        return builder.build();
    }
}
