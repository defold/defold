// Copyright 2020-2024 The Defold Foundation
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

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.graphics.proto.Graphics.VertexAttribute;
import com.google.protobuf.ByteString;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.List;

public class GraphicsUtil {

    private static void validateAttribute(VertexAttribute attr, VertexAttribute.DataType dataType, boolean normalize) throws CompileExceptionError {
        if (normalize || dataType == VertexAttribute.DataType.TYPE_FLOAT) {
            if (!attr.hasDoubleValues()) {
                throw new CompileExceptionError(
                        String.format("Invalid vertex attribute configuration for attribute '%s', data type is %s%s but double_values has not been set.",
                                attr.getName(),
                                normalize ? "normalized " : "",
                                dataType));
            }
        } else {
            if (!attr.hasLongValues()) {
                throw new CompileExceptionError(
                        String.format("Invalid vertex attribute configuration for attribute '%s', data type is %s but long_values has not been set.",
                                attr.getName(),
                                dataType));
            }
        }
    }

    private static void putFloatList(ByteBuffer buffer, List<Double> values) {
        for (double n : values) {
            float v = (float) n;
            buffer.putFloat(v);
        }
    }

    private static void putIntList(ByteBuffer buffer, List<Long> values) {
        for (long n : values) {
            int v = (int) n;
            buffer.putInt(v);
        }
    }

    private static void putShortList(ByteBuffer buffer, List<Long> values) {
        for (long n : values) {
            short v = (short) n;
            buffer.putShort(v);
        }
    }

    private static void putByteList(ByteBuffer buffer, List<Long> values) {
        for (long n : values) {
            byte v = (byte) n;
            buffer.put(v);
        }
    }

    // JG: Maybe shouldn't be here but in a more non-pipeline-util-esque location
    public static ByteBuffer newByteBuffer(int byteCount) {
        ByteBuffer bb = ByteBuffer.allocateDirect(byteCount);
        return bb.order(ByteOrder.LITTLE_ENDIAN);
    }

    private static ByteBuffer makeFloatingPointByteBuffer(List<Double> values) {
        ByteBuffer buffer = newByteBuffer(values.size() * 4);
        putFloatList(buffer, values);
        buffer.rewind();
        return buffer;
    }

    private static ByteBuffer makeIntegerByteBuffer(List<Long> values, VertexAttribute.DataType dataType) {
        ByteBuffer buffer = null;

        switch (dataType) {
            case TYPE_UNSIGNED_INT:
            case TYPE_INT:
                buffer = newByteBuffer(values.size() * 4);
                putIntList(buffer, values);
                break;
            case TYPE_UNSIGNED_SHORT:
            case TYPE_SHORT:
                buffer = newByteBuffer(values.size() * 2);
                putShortList(buffer, values);
                break;
            case TYPE_UNSIGNED_BYTE:
            case TYPE_BYTE:
                buffer = newByteBuffer(values.size());
                putByteList(buffer, values);
                break;
            default:
                assert false;
        }

        buffer.rewind();
        return buffer;
    }

    private static ArrayList<Long> unnormalizeDoubleList(List<Double> doubleList, VertexAttribute.DataType dataType) {
        int count = doubleList.size();
        ArrayList<Long> result = new ArrayList<Long>(count);

        switch (dataType) {
            case TYPE_UNSIGNED_INT:
                for (double n : doubleList) {
                    result.add(Math.round(n * 4294967295.0));
                }
                break;
            case TYPE_INT:
                for (double n : doubleList) {
                    result.add(Math.round(n * (n > 0.0 ? 2147483647.0 : 2147483648.0)));
                }
                break;
            case TYPE_UNSIGNED_SHORT:
                for (double n : doubleList) {
                    result.add(Math.round(n * 65535.0));
                }
                break;
            case TYPE_SHORT:
                for (double n : doubleList) {
                    result.add(Math.round(n * (n > 0.0 ? 32767.0 : 32768.0)));
                }
                break;
            case TYPE_UNSIGNED_BYTE:
                for (double n : doubleList) {
                    result.add(Math.round(n * 255.0));
                }
                break;
            case TYPE_BYTE:
                for (double n : doubleList) {
                    result.add(Math.round(n * (n > 0.0 ? 127.0 : 128.0)));
                }
                break;
            default:
                assert false;
        }

        return result;
    }

    private static ByteBuffer makeByteBuffer(VertexAttribute attr, VertexAttribute.DataType dataType, boolean normalize) {
        if (dataType == VertexAttribute.DataType.TYPE_FLOAT) {
            List<Double> values = attr.getDoubleValues().getVList();
            return makeFloatingPointByteBuffer(values);
        } else if (normalize) {
            List<Double> doubleValues = attr.getDoubleValues().getVList();
            List<Long> values = unnormalizeDoubleList(doubleValues, dataType);
            return makeIntegerByteBuffer(values, dataType);
        } else {
            List<Long> values = attr.getLongValues().getVList();
            return makeIntegerByteBuffer(values, dataType);
        }
    }

    private static ByteString makeBinaryValues(VertexAttribute attr, VertexAttribute.DataType dataType, boolean normalize) {
        ByteBuffer buffer = makeByteBuffer(attr, dataType, normalize);
        return ByteString.copyFrom(buffer);
    }

    public static VertexAttribute getAttributeByName(List<VertexAttribute> materialAttributes, String attributeName)
    {
        for (VertexAttribute attr : materialAttributes) {
            if (attr.getName().equals(attributeName)) {
                return attr;
            }
        }
        return null;
    }

    public static VertexAttribute buildVertexAttribute(VertexAttribute sourceAttr, VertexAttribute targetAttr) throws CompileExceptionError {
        VertexAttribute.DataType dataType = targetAttr.getDataType();
        boolean normalize = targetAttr.getNormalize();
        validateAttribute(sourceAttr, dataType, normalize);
        VertexAttribute.Builder attributeBuilder = VertexAttribute.newBuilder(sourceAttr);
        attributeBuilder.setNameHash(MurmurHash.hash64(sourceAttr.getName()));
        attributeBuilder.setBinaryValues(makeBinaryValues(sourceAttr, dataType, normalize));
        return attributeBuilder.build();
    }
}
