package com.dynamo.bob.pipeline;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.List;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.IResource;
import com.dynamo.bob.Task;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.lua.proto.Lua.LuaModule;
import com.dynamo.lua.proto.Lua.LuaModule.Type;
import com.dynamo.properties.proto.PropertiesProto.PropertyDeclarationEntry;
import com.dynamo.properties.proto.PropertiesProto.PropertyDeclarations;
import com.google.protobuf.ByteString;
import com.google.protobuf.Message;

/**
 * Lua builder. This class is abstract. Inherit from this class
 * and add appropriate {@link BuilderParams}
 * @author chmu
 *
 */
public abstract class LuaBuilder extends Builder<Void> {

    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
        Task<Void> task = Task.<Void>newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(params.outExt()))
                .build();
        return task;
    }

    @Override
    public void build(Task<Void> task) throws CompileExceptionError, IOException {

        LuaModule.Builder builder = LuaModule.newBuilder();
        byte[] scriptBytes = task.input(0).getContent();
        String script = new String(scriptBytes);
        List<String> modules = LuaScanner.scan(script);

        for (String module : modules) {
            String module_file = String.format("/%s.lua", module.replaceAll("\\.", "/"));
            BuilderUtil.checkFile(this.project, task.input(0), "module", module_file);
            builder.addModules(module);
            builder.addResources(module_file + "c");
        }
        Map<String, String> properties = LuaScanner.scanProperties(script);
        PropertyDeclarations propertiesMsg = parseProperties(task.input(0), properties);
        builder.setProperties(propertiesMsg);
        builder.setType(Type.TYPE_TEXT);
        builder.setScript(ByteString.copyFrom(scriptBytes));
        Message msg = builder.build();
        ByteArrayOutputStream out = new ByteArrayOutputStream(4 * 1024);
        msg.writeTo(out);
        out.close();
        task.output(0).setContent(out.toByteArray());
    }

    // http://docs.python.org/dev/library/re.html#simulating-scanf
    private static Pattern numPattern = Pattern.compile("[-+]?(\\d+(\\.\\d*)?|\\.\\d+)([eE][-+]?\\d+)?");
    private static Pattern hashPattern = Pattern.compile("hash\\(\"(.*?)\"\\)");
    private static Pattern urlPattern = Pattern.compile("msg\\.url\\((\"(.*?)\")?\\)");
    private static Pattern vec3Pattern = Pattern.compile("vmath\\.vector3\\(((.*?),(.*?),(.*?)|)\\)");
    private static Pattern vec4Pattern = Pattern.compile("vmath\\.vector4\\(((.*?),(.*?),(.*?),(.*?)|)\\)");
    private static Pattern quatPattern = Pattern.compile("vmath\\.quat\\(((.*?),(.*?),(.*?),(.*?)|)\\)");

    private PropertyDeclarations parseProperties(IResource resource, Map<String, String> properties) throws CompileExceptionError {
        PropertyDeclarations.Builder builder = PropertyDeclarations.newBuilder();
        if (!properties.isEmpty()) {
            for (Map.Entry<String, String> entry : properties.entrySet()) {
                Matcher numMatcher = numPattern.matcher(entry.getValue());
                Matcher hashMatcher = hashPattern.matcher(entry.getValue());
                Matcher urlMatcher = urlPattern.matcher(entry.getValue());
                Matcher vec3Matcher = vec3Pattern.matcher(entry.getValue());
                Matcher vec4Matcher = vec4Pattern.matcher(entry.getValue());
                Matcher quatMatcher = quatPattern.matcher(entry.getValue());
                PropertyDeclarationEntry.Builder entryBuilder = PropertyDeclarationEntry.newBuilder();
                entryBuilder.setKey(entry.getKey());
                entryBuilder.setId(MurmurHash.hash64(entry.getKey()));
                if (numMatcher.matches()) {
                    entryBuilder.setIndex(builder.getFloatValuesCount());
                    builder.addFloatValues(Float.parseFloat(numMatcher.group(0)));
                    builder.addNumberEntries(entryBuilder);
                } else if (hashMatcher.matches()) {
                    entryBuilder.setIndex(builder.getHashValuesCount());
                    builder.addHashValues(MurmurHash.hash64(hashMatcher.group(1)));
                    builder.addHashEntries(entryBuilder);
                } else if (urlMatcher.matches()) {
                    entryBuilder.setIndex(builder.getStringValuesCount());
                    String url = urlMatcher.group(2);
                    if (url == null) {
                        url = "";
                    }
                    builder.addStringValues(url);
                    builder.addUrlEntries(entryBuilder);
                } else if (vec3Matcher.matches()) {
                    entryBuilder.setIndex(builder.getFloatValuesCount());
                    if (vec3Matcher.group(2) != null) {
                        builder.addFloatValues(Float.parseFloat(vec3Matcher.group(2)));
                        builder.addFloatValues(Float.parseFloat(vec3Matcher.group(3)));
                        builder.addFloatValues(Float.parseFloat(vec3Matcher.group(4)));
                    } else {
                        builder.addFloatValues(0.0f);
                        builder.addFloatValues(0.0f);
                        builder.addFloatValues(0.0f);
                    }
                    builder.addVector3Entries(entryBuilder);
                } else if (vec4Matcher.matches()) {
                    entryBuilder.setIndex(builder.getFloatValuesCount());
                    if (vec4Matcher.group(2) != null) {
                        builder.addFloatValues(Float.parseFloat(vec4Matcher.group(2)));
                        builder.addFloatValues(Float.parseFloat(vec4Matcher.group(3)));
                        builder.addFloatValues(Float.parseFloat(vec4Matcher.group(4)));
                        builder.addFloatValues(Float.parseFloat(vec4Matcher.group(5)));
                    } else {
                        builder.addFloatValues(0.0f);
                        builder.addFloatValues(0.0f);
                        builder.addFloatValues(0.0f);
                        builder.addFloatValues(0.0f);
                    }
                    builder.addVector4Entries(entryBuilder);
                } else if (quatMatcher.matches()) {
                    entryBuilder.setIndex(builder.getFloatValuesCount());
                    if (quatMatcher.group(2) != null) {
                        builder.addFloatValues(Float.parseFloat(quatMatcher.group(2)));
                        builder.addFloatValues(Float.parseFloat(quatMatcher.group(3)));
                        builder.addFloatValues(Float.parseFloat(quatMatcher.group(4)));
                        builder.addFloatValues(Float.parseFloat(quatMatcher.group(5)));
                    } else {
                        builder.addFloatValues(0.0f);
                        builder.addFloatValues(0.0f);
                        builder.addFloatValues(0.0f);
                        builder.addFloatValues(0.0f);
                    }
                    builder.addQuatEntries(entryBuilder);
                } else {
                    // TODO Proper error handling
                    throw new CompileExceptionError(resource, 0, "Unknown type");
                }
            }
        }
        return builder.build();
    }
}

