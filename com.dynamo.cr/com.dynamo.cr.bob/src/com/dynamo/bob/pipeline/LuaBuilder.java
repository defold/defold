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

import com.defold.extension.pipeline.ILuaObfuscator;
import com.defold.extension.pipeline.ILuaPreprocessor;
import com.dynamo.bob.Bob;
import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Project;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.logging.Logger;
import com.dynamo.bob.pipeline.LuaScanner.Property.Status;
import com.dynamo.bob.plugin.PluginScanner;
import com.dynamo.bob.util.Exec;
import com.dynamo.bob.util.Exec.Result;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.bob.util.PropertiesUtil;
import com.dynamo.lua.proto.Lua.LuaModule;
import com.dynamo.properties.proto.PropertiesProto.PropertyDeclarationEntry;
import com.dynamo.properties.proto.PropertiesProto.PropertyDeclarations;
import com.dynamo.script.proto.Lua.LuaSource;
import com.google.protobuf.ByteString;
import com.google.protobuf.Message;
import org.apache.commons.io.FileUtils;
import org.apache.commons.io.IOUtils;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector4d;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.RandomAccessFile;
import java.nio.file.Files;
import java.nio.file.attribute.BasicFileAttributes;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Lua builder. This class is abstract. Inherit from this class
 * and add appropriate {@link BuilderParams}
 * @author chmu
 *
 */
public abstract class LuaBuilder extends Builder {

    private static Logger logger = Logger.getLogger(LuaBuilder.class.getName());

    private static ArrayList<Platform> LUA51_PLATFORMS = new ArrayList<Platform>(Arrays.asList(Platform.JsWeb, Platform.WasmWeb, Platform.WasmPthreadWeb));
    private static boolean useLua51;
    private static String luaJITExePath;

    private static List<ILuaPreprocessor> luaPreprocessors = null;
    private static List<ILuaObfuscator> luaObfuscators = null;

    private LuaScanner.Result luaScannerResult;

    /**
     * Get a LuaScanner instance for a resource
     * This will cache the LuaScanner instance per resource to avoid parsing the
     * resource more than once
     * @param resource The resource to get a LuaScanner for
     * @return A LuaScanner instance
     */
    private LuaScanner.Result getLuaScannerResult(IResource resource) throws IOException, CompileExceptionError {
        final String path = resource.getAbsPath();
        final String variant = project.option("variant", Bob.VARIANT_RELEASE);
        if (luaScannerResult == null) {
            final byte[] scriptBytes = resource.getContent();
            String script = new String(scriptBytes, "UTF-8");

            // Create and run preprocessors if some exists.
            if (luaPreprocessors == null) {
                luaPreprocessors = PluginScanner.getOrCreatePlugins("com.defold.extension.pipeline", ILuaPreprocessor.class);

                if (luaPreprocessors == null) {
                    luaPreprocessors = new ArrayList<ILuaPreprocessor>(0);
                }
            }

            for (ILuaPreprocessor luaPreprocessor : luaPreprocessors) {
                try {
                    script = luaPreprocessor.preprocess(script, path, variant);
                }
                catch (Exception e) {
                    throw new CompileExceptionError(resource, 0, "Unable to run Lua preprocessor", e);
                }
            }

            luaScannerResult = LuaScanner.parse(script, Bob.VARIANT_DEBUG.equals(variant));
            for (LuaScanner.ParseError error : luaScannerResult.errors()) {
                throw new CompileExceptionError(resource, error.startLine() + 1, error.message());
            }
        }
        return luaScannerResult;
    }

    @Override
    public Task create(IResource input) throws IOException, CompileExceptionError {
        Task.TaskBuilder taskBuilder = Task.newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(params.outExt()));

        LuaScanner.Result result = getLuaScannerResult(input);
        long finalLuaHash = MurmurHash.hash64(result.code());
        taskBuilder.addExtraCacheKey(Long.toString(finalLuaHash));

        for (LuaScanner.Property property : result.properties()) {
            if (property.isResource()) {
                String value = (String) property.value();

                if (value.isEmpty()) {
                    continue;
                } else if (!PropertiesUtil.isResourceProperty(project, property.type(), value)) {
                    throw new IOException(String.format("Resource '%s' referenced from script resource property '%s' does not exist", value, property.name()));
                }

                createSubTask(value, property.name(), taskBuilder);
            }
        }

        for (String module : result.modules()) {
            String module_file = String.format("/%s.lua", module.replaceAll("\\.", "/"));
            createSubTask(module_file, "Lua module", taskBuilder);
        }

        // Create obfuscators if some exists.
        if (luaObfuscators == null) {
            luaObfuscators = PluginScanner.getOrCreatePlugins("com.defold.extension.pipeline", ILuaObfuscator.class);

            if (luaObfuscators == null) {
                luaObfuscators = new ArrayList<ILuaObfuscator>(0);
            }
        }

        // check if the platform is using Lua 5.1 or LuaJIT
        // get path of LuaJIT executable if the platform uses LuaJIT
        useLua51 = LUA51_PLATFORMS.contains(this.project.getPlatform());
        luaJITExePath = Bob.getHostExeOnce("luajit-64", luaJITExePath);

        return taskBuilder.build();
    }

    private long getSize(File f) throws IOException {
        BasicFileAttributes attr = Files.readAttributes(f.toPath(), BasicFileAttributes.class);
        return attr.size();
    }
    private void writeToFile(File f, byte[] b) throws CompileExceptionError, IOException {
        InputStream in = new ByteArrayInputStream(b);
        FileUtils.copyInputStreamToFile(in, f);
        try {
            int retries = 40;
            while (getSize(f) < b.length) {
                logger.info("File '%s' isn't written. Retry %d.", f.getPath(), retries);
                Thread.sleep(50);
                if (--retries == 0) {
                    throw new CompileExceptionError(String.format("File '%s' is not of the expected size", f.getPath()));
                }
            }
        }
        catch (java.lang.InterruptedException e) {
            e.printStackTrace();
        }
    }

    private int executeProcess(Task task, List<String> options, Map<String, String> env, File inputFile, int retrievableFailure) throws IOException, CompileExceptionError {
        Result r = Exec.execResultWithEnvironment(env, options);
        int ret = r.ret;
        String cmdOutput = new String(r.stdOutErr);
        if (ret == retrievableFailure) {
            return retrievableFailure;
        }
        if (ret != 0) {
            logger.info("Bytecode construction failed with exit code %d", ret);

            // Parse and handle the error output
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
            } else {
                System.out.printf("Lua Error: for file %s: '%s'\n", task.input(0).getPath(), cmdOutput);
            }
            // Since parsing out the actual error failed, as a backup just
            // spit out whatever luajit/luac said.
            inputFile.delete();
            throw new CompileExceptionError(task.input(0), 1, cmdOutput);
        }

        return ret;
    }

    public byte[] constructBytecode(Task task, String source, File inputFile, File outputFile, List<String> options, Map<String, String> env) throws IOException, CompileExceptionError {
        FileOutputStream fo = null;
        RandomAccessFile rdr = null;

        try {
            // Need to write the input file separately in case it comes from built-in, and cannot
            // be found through its path alone.
            writeToFile(inputFile, source.getBytes());

            int maxRetries = 10;
            int retries = 0;
            int retrievableFailure = 139; // Segmentation fault or similar retrievable failure

            while (retries < maxRetries) {
                int ret = executeProcess(task, options, env, inputFile, retrievableFailure);
                if (ret == retrievableFailure) {
                    retries++;
                    logger.info("Attempt %d failed with exit code %d, retrying...", retries, retrievableFailure);
                    try {
                        Thread.sleep(50); // Pause before retrying
                    } catch (InterruptedException e) {
                        logger.severe("Unexpected interruption during retry", e);
                    }
                } else {
                    // Process completed successfully or failed with a non-retriable code
                    break;
                }
            }

            if (retries == maxRetries) {
                throw new RuntimeException(String.format("Exceeded maximum retry attempts (%d) due to repeated exit code %d.", maxRetries, retrievableFailure));
            }

            // Read output file contents
            long resultBytes = outputFile.length();
            rdr = new RandomAccessFile(outputFile, "r");
            byte[] tmp = new byte[(int) resultBytes];
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
    private String getChunkName(Task task) {
        String chunkName = "@" + task.input(0).getPath();
        return chunkName;
    }

    /* We currently prefer source code over plain lua byte code due to the smaller size
    public byte[] constructLuaBytecode(Task task, String luacExe, String source) throws IOException, CompileExceptionError {
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
    */

    public byte[] constructLuaJITBytecode(Task task, String source, boolean gen32bit) throws IOException, CompileExceptionError {

        File outputFile = File.createTempFile("script", ".raw");
        File inputFile = File.createTempFile("script", ".lua");

        // -b = generate bytecode
        // 
        // -W = Generate 32 bit (non-GC64) bytecode.
        // -X = Generate 64 bit (GC64) bytecode.
        // -d = Generate bytecode in deterministic manner.
        // 
        // -F = supply the correct chunk name (the original source file)
        //      The chunk name is prefixed with @ so that the last 60
        //      characters of the name are shown. See implementation of
        //      luaO_chunkid in lobject.c. If a script error occurs in
        //      runtime we want Lua to report the end of the filepath
        //      associated with the chunk, since this is where the filename
        //      is visible.
        //
        // -g = keep debug info
        final String chunkName = getChunkName(task);
        List<String> options = new ArrayList<String>();
        options.add(luaJITExePath);
        options.add("-b");
        options.add("-d");
        options.add("-g");
        options.add(gen32bit ? "-W" : "-X");
        options.add("-F"); options.add(task.input(0).getPath());
        options.add(inputFile.getAbsolutePath());
        options.add(outputFile.getAbsolutePath());

        Map<String, String> env = new HashMap<String, String>();
        env.put("LUA_PATH", Bob.getPath("share/luajit/") + "/?.lua");

        return constructBytecode(task, source, inputFile, outputFile, options, env);
    }

    public byte[] constructBytecodeDelta(byte[] bytecode64, byte[] bytecode32) throws CompileExceptionError
    {
        // expect same length on 32 and 64 bit bytecode if storing a delta
        if (bytecode32.length != bytecode64.length) {
            throw new CompileExceptionError("Byte code length mismatch");
        }

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
            while(count < 255 && (i + count) < bytecode32.length) {
                if (bytecode32[i + count] == bytecode64[i + count]) {
                    break;
                }
                count++;
            }

            // found a sequence of bytes
            // write index, count and bytes
            if (count > 0) {
                if (count == 256) {
                    logger.info("\n\nLuaBuilder count %d\n\n", count);
                }

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
        return delta;
    }

    @Override
    public void build(Task task) throws CompileExceptionError, IOException {

        LuaModule.Builder builder = LuaModule.newBuilder();

        // get and remove require and properties from LuaScanner
        LuaScanner.Result result = getLuaScannerResult(task.firstInput());
        String script = result.code();
        List<String> modules = result.modules();
        List<LuaScanner.Property> properties = result.properties();

        // add detected modules to builder
        for (String module : modules) {
            String module_file = String.format("/%s.lua", module.replaceAll("\\.", "/"));
            BuilderUtil.checkResource(this.project, task.firstInput(), "module", module_file);
            builder.addModules(module);
            builder.addResources(module_file + "c");
        }

        // add detected properties to builder
        Collection<String> propertyResources = new HashSet<String>();
        PropertyDeclarations propertiesMsg = buildProperties(task.firstInput(), properties, propertyResources);
        builder.setProperties(propertiesMsg);
        builder.addAllPropertyResources(propertyResources);

        // apply obfuscation
        final IResource sourceResource = task.firstInput();
        final String sourcePath = sourceResource.getAbsPath();
        final String variant = project.option("variant", Bob.VARIANT_RELEASE);

        for (ILuaObfuscator luaObfuscator : luaObfuscators) {
            try {
                script = luaObfuscator.obfuscate(script, sourcePath, variant);
            }
            catch (Exception e) {
                throw new CompileExceptionError(sourceResource, 0, "Unable to run Lua obfuscator", e);
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
        if (useLua51) {
            constructLuaJITBytecode(task, script, false);
            srcBuilder.setScript(ByteString.copyFrom(script.getBytes()));
        }
        // include uncompressed Lua source code instead of bytecode
        // see https://forum.defold.com/t/urgent-need-help-i-have-huge-problem-with-game-submission-to-apple/68031
        else if (useUncompressedLuaSource) {
            constructLuaJITBytecode(task, script, false);
            srcBuilder.setScript(ByteString.copyFrom(script.getBytes()));
        }
        else {
            boolean useLuaBytecodeDelta = this.project.option("use-lua-bytecode-delta", "false").equals("true");

            // We may have multiple archs with same bitness
            boolean needs32bit = false;
            boolean needs64bit = false;
            List<Platform> architectures = project.getArchitectures();
            for (Platform platform : architectures)
            {
                if (platform.is64bit())
                    needs64bit = true;
                else
                    needs32bit = true;
            }

            byte[] bytecode32 = new byte[0];
            byte[] bytecode64 = new byte[0];
            if (needs32bit)
                bytecode32 = constructLuaJITBytecode(task, script, true);
            if (needs64bit)
                bytecode64 = constructLuaJITBytecode(task, script, false);

            if ( needs32bit ^ needs64bit ) { // if only one of them is set
                if (needs64bit) {
                    srcBuilder.setBytecode(ByteString.copyFrom(bytecode64));
                    logger.fine("Writing 64-bit bytecode without delta for %s", task.input(0).getPath());
                }
                else {
                    srcBuilder.setBytecode(ByteString.copyFrom(bytecode32));
                    logger.fine("Writing 32-bit bytecode without delta for %s", task.input(0).getPath());
                }
            }
            else if (!useLuaBytecodeDelta) {
                logger.fine("Writing 32 and 64-bit bytecode for %s", task.input(0).getPath());
                srcBuilder.setBytecode32(ByteString.copyFrom(bytecode32));
                srcBuilder.setBytecode64(ByteString.copyFrom(bytecode64));
            }
            else {
                byte[] delta = constructBytecodeDelta(bytecode32, bytecode64);
                srcBuilder.setDelta(ByteString.copyFrom(delta));

                logger.fine("Writing 64-bit bytecode with 32-bit delta for %s", task.input(0).getPath());
                srcBuilder.setBytecode(ByteString.copyFrom(bytecode64));
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
                if (property.status() == Status.OK) {
                    PropertyDeclarationEntry.Builder entryBuilder = PropertyDeclarationEntry.newBuilder();
                    entryBuilder.setKey(property.name());
                    entryBuilder.setId(MurmurHash.hash64(property.name()));
                    switch (property.type()) {
                    case PROPERTY_TYPE_NUMBER:
                        entryBuilder.setIndex(builder.getFloatValuesCount());
                        builder.addFloatValues(((Double)property.value()).floatValue());
                        builder.addNumberEntries(entryBuilder);
                        break;
                    case PROPERTY_TYPE_HASH:
                        String value = (String)property.value();
                        if (PropertiesUtil.isResourceProperty(project, property.type(), value)) {
                            value = PropertiesUtil.transformResourcePropertyValue(resource, value);
                            propertyResources.add(value);
                        }
                        entryBuilder.setIndex(builder.getHashValuesCount());
                        builder.addHashValues(MurmurHash.hash64(value));
                        builder.addHashEntries(entryBuilder);
                        break;
                    case PROPERTY_TYPE_URL:
                        entryBuilder.setIndex(builder.getStringValuesCount());
                        builder.addStringValues((String)property.value());
                        builder.addUrlEntries(entryBuilder);
                        break;
                    case PROPERTY_TYPE_VECTOR3:
                        entryBuilder.setIndex(builder.getFloatValuesCount());
                        entryBuilder.addElementIds(MurmurHash.hash64(property.name() + ".x"));
                        entryBuilder.addElementIds(MurmurHash.hash64(property.name() + ".y"));
                        entryBuilder.addElementIds(MurmurHash.hash64(property.name() + ".z"));
                        Vector3d v3 = (Vector3d)property.value();
                        builder.addFloatValues((float)v3.getX());
                        builder.addFloatValues((float)v3.getY());
                        builder.addFloatValues((float)v3.getZ());
                        builder.addVector3Entries(entryBuilder);
                        break;
                    case PROPERTY_TYPE_VECTOR4:
                        entryBuilder.setIndex(builder.getFloatValuesCount());
                        entryBuilder.addElementIds(MurmurHash.hash64(property.name() + ".x"));
                        entryBuilder.addElementIds(MurmurHash.hash64(property.name() + ".y"));
                        entryBuilder.addElementIds(MurmurHash.hash64(property.name() + ".z"));
                        entryBuilder.addElementIds(MurmurHash.hash64(property.name() + ".w"));
                        Vector4d v4 = (Vector4d) property.value();
                        builder.addFloatValues((float)v4.getX());
                        builder.addFloatValues((float)v4.getY());
                        builder.addFloatValues((float)v4.getZ());
                        builder.addFloatValues((float)v4.getW());
                        builder.addVector4Entries(entryBuilder);
                        break;
                    case PROPERTY_TYPE_QUAT:
                        entryBuilder.setIndex(builder.getFloatValuesCount());
                        entryBuilder.addElementIds(MurmurHash.hash64(property.name() + ".x"));
                        entryBuilder.addElementIds(MurmurHash.hash64(property.name() + ".y"));
                        entryBuilder.addElementIds(MurmurHash.hash64(property.name() + ".z"));
                        entryBuilder.addElementIds(MurmurHash.hash64(property.name() + ".w"));
                        Quat4d q = (Quat4d) property.value();
                        builder.addFloatValues((float)q.getX());
                        builder.addFloatValues((float)q.getY());
                        builder.addFloatValues((float)q.getZ());
                        builder.addFloatValues((float)q.getW());
                        builder.addQuatEntries(entryBuilder);
                        break;
                    case PROPERTY_TYPE_BOOLEAN:
                        entryBuilder.setIndex(builder.getFloatValuesCount());
                        builder.addFloatValues(((Boolean) property.value()) ? 1.0f : 0.0f);
                        builder.addBoolEntries(entryBuilder);
                        break;
                    }
                } else if (property.status() == Status.INVALID_ARGS) {
                    throw new CompileExceptionError(resource, property.startLine() + 1, "go.property takes a string and a value as arguments. The value must have the type number, boolean, hash, msg.url, vmath.vector3, vmath.vector4, vmath.quat, or resource.*.");
                } else if (property.status() == Status.INVALID_VALUE) {
                    throw new CompileExceptionError(resource, property.startLine() + 1, "Only these types are available: number, hash, msg.url, vmath.vector3, vmath.vector4, vmath.quat, resource.*");
                } else if (property.status() == Status.INVALID_LOCATION) {
                    throw new CompileExceptionError(resource, property.startLine() + 1, "go.property should be a top-level statement");
                }
            }
        }
        return builder.build();
    }

    @Override
    public void clearState() {
        super.clearState();
        luaScannerResult = null;
    }
}
