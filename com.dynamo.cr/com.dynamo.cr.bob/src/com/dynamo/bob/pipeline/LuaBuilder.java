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
import java.io.FileOutputStream;
import java.util.Collection;
import java.util.Map;
import java.util.HashMap;
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

    private static ArrayList<Platform> platformUsesLua51 = new ArrayList<Platform>(Arrays.asList(Platform.JsWeb, Platform.WasmWeb));

    private static LuaBuilderPlugin luaBuilderPlugin = null;

    private Map<String, LuaScanner> luaScanners = new HashMap();

    /**
     * Get a LuaScanner instance for a resource
     * This will cache the LuaScanner instance per resource to avoid parsing the
     * resource more than once
     * @param resource The resource to get a LuaScanner for
     * @return A LuaScanner instance
     */
    private LuaScanner getLuaScanner(IResource resource) throws IOException {
        final String path = resource.getAbsPath();
        LuaScanner scanner = luaScanners.get(path);
        if (scanner == null) {
            final byte[] scriptBytes = resource.getContent();
            final String script = new String(scriptBytes, "UTF-8");
            scanner = new LuaScanner();
            scanner.parse(script);
            luaScanners.put(path, scanner);
        }
        return scanner;
    }

    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
        Task.TaskBuilder<Void> taskBuilder = Task.<Void>newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(params.outExt()));

        LuaScanner scanner = getLuaScanner(input);

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

    public byte[] constructBytecode(Task<Void> task, String source, File inputFile, File outputFile, List<String> options, Map<String, String> env) throws IOException, CompileExceptionError {
        FileOutputStream fo = null;
        RandomAccessFile rdr = null;

        try {
            Bob.initLua(); // unpack the lua resources

            // Need to write the input file separately in case it comes from built-in, and cannot
            // be found through its path alone.
            fo = new java.io.FileOutputStream(inputFile);
            fo.write(source.getBytes());
            fo.close();

            ProcessBuilder pb = new ProcessBuilder(options).redirectErrorStream(true);
            pb.environment().putAll(env);

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
                    // first delimiter is the executable name "luajit:" or "luac:"
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
                    else {
                        System.out.printf("Lua Error: for file %s: '%s'\n", task.input(0).getPath(), cmdOutput);
                    }
                    // Since parsing out the actual error failed, as a backup just
                    // spit out whatever luajit/luac said.
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

    // we use the same chunk name across the board
    // we always use @ + full path
    // if the path is shorter than 60 characters the runtime will show the full path
    // if the path is longer than 60 characters the runtime will show ... and the last portion of the path
    private String getChunkName(Task<Void> task) {
        String chunkName = "@" + task.input(0).getPath();
        return chunkName;
    }

    public byte[] constructLuaBytecode(Task<Void> task, String luacExe, String source) throws IOException, CompileExceptionError {
        File outputFile = File.createTempFile("script", ".raw");
        File inputFile = File.createTempFile("script", ".lua");

        List<String> options = new ArrayList<String>();
        options.add(Bob.getExe(Platform.getHostPlatform(), luacExe));
        options.add("-o"); options.add(outputFile.getAbsolutePath());
        options.add(inputFile.getAbsolutePath());

        Map<String, String> env = new HashMap<String, String>();

        byte[] bytecode = constructBytecode(task, source, inputFile, outputFile, options, env);

        // we need to patch the bytecode with the proper filename for the chunk and not the
        // temporary filename (outputFile created above)
        //
        // we do this by first reading the length of the tmp name from the bytecode
        // next we create the actual name from the original filename, taken from the task
        // finally we create the new bytecode

        final int LUAC_HEADERSIZE = 12; // from lundump.c

        // read length of tmp chunkname
        final int tmpChunkNameLength = bytecode[LUAC_HEADERSIZE]
            + (bytecode[LUAC_HEADERSIZE + 1] << 8)
            + (bytecode[LUAC_HEADERSIZE + 2] << 16)
            + (bytecode[LUAC_HEADERSIZE + 3] << 24);

        // the new chunkname, created from the original filename
        final String chunkName = getChunkName(task);
        final byte[] chunkNameBytes = chunkName.getBytes();
        final int chunkNameLength = chunkNameBytes.length;

        // create new bytecode
        // write Lua header
        // write real chunkname length
        // write real chunkname bytes + null termination
        // write rest of bytecode
        ByteArrayOutputStream baos = new ByteArrayOutputStream(bytecode.length - tmpChunkNameLength + chunkNameLength + 1);
        baos.write(bytecode, 0, LUAC_HEADERSIZE);
        baos.write(chunkNameLength & 0xff);
        baos.write((chunkNameLength >> 8) & 0xff);
        baos.write((chunkNameLength >> 16) & 0xff);
        baos.write((chunkNameLength >> 24) & 0xff);
        baos.write(chunkNameBytes);
        baos.write(0);
        baos.write(bytecode, 12 + 4 + tmpChunkNameLength + 1, bytecode.length - LUAC_HEADERSIZE - 4 - tmpChunkNameLength - 1);

        // get new bytes
        bytecode = baos.toByteArray();
        baos.close();
        return bytecode;
    }

    public byte[] constructLuaJITBytecode(Task<Void> task, String luajitExe, String source) throws IOException, CompileExceptionError {

        Bob.initLua(); // unpack the lua resources

        File outputFile = File.createTempFile("script", ".raw");
        File inputFile = File.createTempFile("script", ".lua");

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
        final String chunkName = getChunkName(task);
        List<String> options = new ArrayList<String>();
        options.add(Bob.getExe(Platform.getHostPlatform(), luajitExe));
        options.add("-b");
        options.add("-g"); // Keep debug info
        options.add("-f"); options.add(chunkName);
        options.add(inputFile.getAbsolutePath());
        options.add(outputFile.getAbsolutePath());

        Map<String, String> env = new HashMap<String, String>();
        env.put("LUA_PATH", Bob.getPath("share/luajit/") + "/?.lua");

        return constructBytecode(task, source, inputFile, outputFile, options, env);
    }

    @Override
    public void build(Task<Void> task) throws CompileExceptionError, IOException {

        LuaModule.Builder builder = LuaModule.newBuilder();

        // get and remove require and properties from LuaScanner
        LuaScanner scanner = getLuaScanner(task.input(0));
        String script = scanner.getParsedLua();
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

        boolean useUncompressedLuaSource = this.project.option("use-uncompressed-lua-source", "false").equals("true");
        // set compression and encryption flags
        // if the use-uncompressed-lua-source flag is set the project will use uncompressed plain text Lua script files
        // if the use-uncompressed-lua-source flag is NOT set the project will use encrypted and possibly also compressed bytecode
        for(IResource res : task.getOutputs()) {
            String path = res.getAbsPath();
            if(path.endsWith("luac") || path.endsWith("scriptc") || path.endsWith("gui_scriptc") || path.endsWith("render_scriptc")) {
                if (useUncompressedLuaSource) {
                    project.addOutputFlags(path, Project.OutputFlags.UNCOMPRESSED);
                }
                else {
                    project.addOutputFlags(path, Project.OutputFlags.ENCRYPTED);
                }
            }
        }

        LuaSource.Builder srcBuilder = LuaSource.newBuilder();
        srcBuilder.setFilename(getChunkName(task));

        // for platforms using Lua 5.1 we include Lua source code even though
        // there is a constructLuaBytecode() function above
        // tests have shown that Lua 5.1 bytecode becomes larger than source,
        // even when compressed using lz4
        // this is unacceptable for html5 games where size is a key factor
        // see https://github.com/defold/defold/issues/6891 for more info
        if (platformUsesLua51.contains(project.getPlatform())) {
            srcBuilder.setScript(ByteString.copyFrom(script.getBytes()));
        }
        // include uncompressed Lua source code instead of bytecode
        // see https://forum.defold.com/t/urgent-need-help-i-have-huge-problem-with-game-submission-to-apple/68031
        else if (useUncompressedLuaSource) {
            srcBuilder.setScript(ByteString.copyFrom(script.getBytes()));
        }
        else {
            byte[] bytecode32 = constructLuaJITBytecode(task, "luajit-32", script);
            byte[] bytecode64 = constructLuaJITBytecode(task, "luajit-64", script);

            if (bytecode32.length != bytecode64.length) {
                throw new CompileExceptionError(task.input(0), 0, "Byte code length mismatch");
            }

            List<Platform> architectures = project.getArchitectures();
            if (architectures.size() == 1) {
                Platform p = architectures.get(0);
                if (p.is64bit()) {
                    srcBuilder.setBytecode(ByteString.copyFrom(bytecode64));
                    Bob.verbose("Writing 64-bit bytecode without delta for %s", task.input(0).getPath());
                }
                else {
                    srcBuilder.setBytecode(ByteString.copyFrom(bytecode32));
                    Bob.verbose("Writing 32-bit bytecode without delta for %s", task.input(0).getPath());
                }
            }
            else {
                Bob.verbose("Writing 64-bit bytecode with 32-bit delta for %s", task.input(0).getPath());
                srcBuilder.setBytecode(ByteString.copyFrom(bytecode64));

                /**
                 * Calculate the difference/delta between the 64-bit and 32-bit
                 * bytecode.
                 * The delta is stored together with the 64-bit bytecode and when
                 * the 32-bit bytecode is needed the delta is applied to the 64-bit
                 * bytecode to transform it to the equivalent 32-bit version.
                 * 
                 * The delta is stored in the following format:
                 * 
                 * * index - The index where to apply the next change. 1-4 bytes.
                 *           The size depends on the size of the entire bytecode:
                 *           1 byte - Size less than 2^8
                 *           2 bytes - Size less than 2^16
                 *           3 bytes - Size less than 2^24
                 *           4 bytes - Size more than or equal to 2^24
                 * * count - The number of consecutive bytes to alter. 1 byte (ie max 255 changes)
                 * * bytes - The 32-bit bytecode values to apply to the 64-bit bytecode starting
                 *           at the index.
                 */
                ByteArrayOutputStream bos = new ByteArrayOutputStream();
                int i = 0;
                while(i < bytecode32.length) {
                    // find sequences of consecutive bytes that differ
                    // max 255 at a time
                    int count = 0;
                    while(count < 256 && (i + count) < bytecode32.length) {
                        if (bytecode32[i + count] == bytecode64[i + count]) {
                            break;
                        }
                        count++;
                    }

                    // found a sequence of bytes
                    // write index, count and bytes
                    if (count > 0) {

                        // write index of diff
                        bos.write(i);
                        if (bytecode32.length >= (1 << 8)) {
                            bos.write((i & 0xFF00) >> 8);
                        }
                        if (bytecode32.length >= (1 << 16)) {
                            bos.write((i & 0xFF0000) >> 16);
                        }
                        if (bytecode32.length >= (1 << 24)) {
                            bos.write((i & 0xFF000000) >> 24);
                        }

                        bos.write(count);

                        // write consecutive bytes that differ
                        bos.write(bytecode32, i, count);
                        i += count;
                    }
                    else {
                        i += 1;
                    }
                }

                byte[] delta = bos.toByteArray();
                srcBuilder.setDelta(ByteString.copyFrom(delta));
            }
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
