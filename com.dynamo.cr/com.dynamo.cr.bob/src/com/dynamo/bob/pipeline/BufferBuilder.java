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

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.Iterator;
import java.util.Arrays;

import org.codehaus.jackson.JsonNode;
import org.codehaus.jackson.JsonParseException;
import org.codehaus.jackson.map.ObjectMapper;

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.util.MurmurHash;

import com.dynamo.gamesys.proto.BufferProto.BufferDesc;
import com.dynamo.gamesys.proto.BufferProto.StreamDesc;
import com.dynamo.gamesys.proto.BufferProto.ValueType;


@BuilderParams(name="Buffer", inExts=".buffer", outExt=".bufferc")
public class BufferBuilder extends Builder {

    static String allowedTypeStrings = "uint8, uint16, uint32, uint64, int8, int16, int32, int64, float32";

    @Override
    public Task create(IResource input) throws IOException, CompileExceptionError {
        return defaultTask(input);
    }

    static ValueType stringTypeToDDFType(String type) throws CompileExceptionError {
        switch (type) {
            case "uint8": return ValueType.VALUE_TYPE_UINT8;
            case "uint16": return ValueType.VALUE_TYPE_UINT16;
            case "uint32": return ValueType.VALUE_TYPE_UINT32;
            case "uint64": return ValueType.VALUE_TYPE_UINT64;
            case "int8": return ValueType.VALUE_TYPE_INT8;
            case "int16": return ValueType.VALUE_TYPE_INT16;
            case "int32": return ValueType.VALUE_TYPE_INT32;
            case "int64": return ValueType.VALUE_TYPE_INT64;
            case "float32": return ValueType.VALUE_TYPE_FLOAT32;
            default:
                return null;
        }
    }

    static Integer[] fillIntArray(StreamDesc.Builder builder, JsonNode dataNode) {
        int dataCount = dataNode.size();
        Integer[] out = new Integer[dataCount];
        for (int i = 0; i < dataCount; ++i) {
            out[i] = (int)dataNode.get(i).asDouble();
        }
        return out;
    }

    static Long[] fillLongArray(StreamDesc.Builder builder, JsonNode dataNode) {
        int dataCount = dataNode.size();
        Long[] out = new Long[dataCount];
        for (int i = 0; i < dataCount; ++i) {
            out[i] = (long)dataNode.get(i).asDouble();
        }
        return out;
    }

    static Float[] fillFloatArray(StreamDesc.Builder builder, JsonNode dataNode) {
        int dataCount = dataNode.size();
        Float[] out = new Float[dataCount];
        for (int i = 0; i < dataCount; ++i) {
            out[i] = (float)dataNode.get(i).asDouble();
        }
        return out;
    }

    static void fillUint(StreamDesc.Builder builder, JsonNode dataNode) {
        builder.addAllUi(Arrays.asList(fillIntArray(builder, dataNode)));
    }

    static void fillInt(StreamDesc.Builder builder, JsonNode dataNode) {
        builder.addAllI(Arrays.asList(fillIntArray(builder, dataNode)));
    }

    static void fillUlong(StreamDesc.Builder builder, JsonNode dataNode) {
        builder.addAllUi64(Arrays.asList(fillLongArray(builder, dataNode)));
    }

    static void fillLong(StreamDesc.Builder builder, JsonNode dataNode) {
        builder.addAllI64(Arrays.asList(fillLongArray(builder, dataNode)));
    }

    static void fillFloat(StreamDesc.Builder builder, JsonNode dataNode) {
        builder.addAllF(Arrays.asList(fillFloatArray(builder, dataNode)));
    }

    static void fillData(StreamDesc.Builder builder, JsonNode dataNode, ValueType type)
    {
        switch (type) {
            case VALUE_TYPE_INT8:
            case VALUE_TYPE_INT16:
            case VALUE_TYPE_INT32:
                fillInt(builder, dataNode);
                break;

            case VALUE_TYPE_UINT8:
            case VALUE_TYPE_UINT16:
            case VALUE_TYPE_UINT32:
                fillUint(builder, dataNode);
                break;

            case VALUE_TYPE_UINT64:
                fillUlong(builder, dataNode);
                break;

            case VALUE_TYPE_INT64:
                fillLong(builder, dataNode);
                break;

            case VALUE_TYPE_FLOAT32:
                fillFloat(builder, dataNode);
                break;
        }
    }

    @Override
    public void build(Task task) throws CompileExceptionError, IOException {
        ByteArrayInputStream bufferJsonIs = new ByteArrayInputStream(task.input(0).getContent());

        BufferDesc.Builder bufferDescBuilder = BufferDesc.newBuilder();

        try {
            ObjectMapper m = new ObjectMapper();
            JsonNode bufferNode = m.readValue(new InputStreamReader(bufferJsonIs, "UTF-8"), JsonNode.class);

            Iterator<JsonNode> streamNodes = bufferNode.getElements();
            while (streamNodes.hasNext()) {
                JsonNode streamNode = streamNodes.next();

                // Get and check that all required fields are available for a stream.

                // name field
                if (streamNode.get("name") == null) {
                    throw new CompileExceptionError(task.input(0), 0, "Stream is missing required name field.");
                }
                String streamName = streamNode.get("name").asText();

                // type field (we also make sure it is a supported type)
                if (streamNode.get("type") == null) {
                    throw new CompileExceptionError(task.input(0), 0, "Stream '" + streamName + "' is missing required type field.");
                }
                String streamTypeString = streamNode.get("type").asText();
                ValueType streamType = stringTypeToDDFType(streamTypeString);
                if (streamType == null) {
                    throw new CompileExceptionError(task.input(0), 0, "Unknown stream type: " + streamTypeString + " (allowed types: " + allowedTypeStrings + ").");
                }

                // count field
                if (streamNode.get("count") == null) {
                    throw new CompileExceptionError(task.input(0), 0, "Stream '" + streamName + "' is missing required count field.");
                }
                int streamValueCount = streamNode.get("count").asInt();

                StreamDesc.Builder streamDescBuilder = StreamDesc.newBuilder();
                streamDescBuilder.setName(streamName);
                streamDescBuilder.setNameHash(MurmurHash.hash64(streamName));
                streamDescBuilder.setValueType(streamType);
                streamDescBuilder.setValueCount(streamValueCount);

                // Fill corresponding protobuf data field depending on what the stream type is.
                JsonNode dataNode = streamNode.get("data");
                if (dataNode != null) {
                    fillData(streamDescBuilder, dataNode, streamType);
                }

                bufferDescBuilder.addStreams(streamDescBuilder.build());
            }
        } catch (JsonParseException e) {
            throw new CompileExceptionError(task.input(0), 0, "JSON error while parsing buffer resource: " + e.getMessage());
        }

        ByteArrayOutputStream out = new ByteArrayOutputStream(64 * 1024);
        bufferDescBuilder.build().writeTo(out);
        out.close();
        task.output(0).setContent(out.toByteArray());
    }
}
