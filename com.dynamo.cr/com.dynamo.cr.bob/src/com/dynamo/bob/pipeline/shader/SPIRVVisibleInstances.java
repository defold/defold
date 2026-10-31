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

package com.dynamo.bob.pipeline.shader;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;

// Replaces one reflected mat4 input with a private matrix loaded at the vertex
// entry point. Working on compiled SPIR-V preserves user shader syntax, helper
// functions, stage interfaces, and the original uniform/texture binding ABI.
final class SPIRVVisibleInstances {
    private static final int OP_ENTRY_POINT = 15, OP_TYPE_INT = 21, OP_TYPE_POINTER = 32;
    private static final int OP_FUNCTION = 54, OP_VARIABLE = 59, OP_LOAD = 61, OP_STORE = 62;
    private static final int OP_ACCESS_CHAIN = 65, OP_IN_BOUNDS_ACCESS_CHAIN = 66, OP_COPY_OBJECT = 83;
    private static final int OP_DECORATE = 71, OP_MEMBER_DECORATE = 72, OP_LABEL = 248;
    private static final int INPUT = 1, UNIFORM = 2, PRIVATE = 6;
    private final ArrayList<int[]> types = new ArrayList<>();
    private final HashMap<Integer, int[]> pointers = new HashMap<>();
    private final HashMap<Integer, Integer> privatePointers = new HashMap<>();
    private int nextId;

    private static int[] op(int opcode, int... operands) {
        int[] words = new int[operands.length + 1];
        words[0] = (words.length << 16) | opcode;
        System.arraycopy(operands, 0, words, 1, operands.length);
        return words;
    }

    private static int[] name(int id, String name) {
        byte[] bytes = (name + "\0").getBytes(java.nio.charset.StandardCharsets.UTF_8);
        ByteBuffer data = ByteBuffer.allocate((bytes.length + 3) / 4 * 4).order(ByteOrder.LITTLE_ENDIAN);
        data.put(bytes); data.rewind();
        int[] operands = new int[1 + data.capacity() / 4]; operands[0] = id;
        for (int i = 1; i < operands.length; ++i) operands[i] = data.getInt();
        return op(5, operands); // OpName
    }

    private int id() { return nextId++; }

    private int privatePointer(int type) {
        Integer pointer = privatePointers.get(type);
        if (pointer == null) {
            pointer = id();
            types.add(op(OP_TYPE_POINTER, pointer, PRIVATE, type));
            privatePointers.put(type, pointer);
        }
        return pointer;
    }

    static byte[] transform(byte[] bytes, int inputId) {
        return new SPIRVVisibleInstances().rewrite(bytes, inputId);
    }

    private byte[] rewrite(byte[] bytes, int inputId) {
        ByteBuffer input = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN);
        int[] header = new int[5];
        for (int i = 0; i < 5; ++i) header[i] = input.getInt();
        // This pass targets the pipeline's Vulkan 1.0 modules. SPIR-V 1.4 adds
        // different entry-point interface rules; leave those on the matrix path.
        if (header[1] > 0x00010300) return null;
        nextId = header[3];
        ArrayList<int[]> instructions = new ArrayList<>();
        int entry = 0, matrix = 0, uintType = 0, intType = 0, instance = 0;
        for (int i = 5; i < bytes.length / 4;) {
            int first = input.getInt();
            int count = first >>> 16;
            int[] words = new int[count];
            words[0] = first;
            for (int j = 1; j < count; ++j) words[j] = input.getInt();
            instructions.add(words);
            int opcode = first & 0xffff;
            if (opcode == OP_TYPE_POINTER) {
                pointers.put(words[1], words);
                if (words[2] == PRIVATE) privatePointers.put(words[3], words[1]);
            }
            if (opcode == OP_TYPE_INT && words[2] == 32) {
                if (words[3] == 0) uintType = words[1]; else intType = words[1];
            }
            if (opcode == OP_VARIABLE && words[2] == inputId) matrix = pointers.get(words[1])[3];
            if (opcode == OP_ENTRY_POINT) entry = words[2];
            if (opcode == OP_DECORATE && words[2] == 11 && words[3] == 43) instance = words[1];
            i += count;
        }
        boolean matrix4 = false;
        for (int[] words : instructions)
            if ((words[0] & 0xffff) == 24 && words[1] == matrix && words[3] == 4) matrix4 = true;
        if (!matrix4) return null;
        if (uintType == 0) { uintType = id(); types.add(op(OP_TYPE_INT, uintType, 32, 0)); }
        if (intType == 0) { intType = id(); types.add(op(OP_TYPE_INT, intType, 32, 1)); }
        int matrixPointer = privatePointer(matrix);
        int zero = id(); types.add(op(43, uintType, zero, 0)); // OpConstant
        ArrayList<int[]> names = new ArrayList<>();
        ArrayList<int[]> annotations = new ArrayList<>();
        ArrayList<int[]> variables = new ArrayList<>();
        boolean newInstance = instance == 0;
        if (newInstance) {
            int pointer = id(); types.add(op(OP_TYPE_POINTER, pointer, INPUT, intType));
            instance = id(); variables.add(op(OP_VARIABLE, pointer, instance, INPUT));
            annotations.add(op(OP_DECORATE, instance, 11, 43)); // BuiltIn InstanceIndex
        }
        // Both buffers occupy a descriptor set unused by the source program.
        int[] buffers = new int[2];
        int[] elements = {matrix, uintType};
        int[] strides = {64, 4};
        for (int i = 0; i < 2; ++i) {
            int array = id(), block = id(), pointer = id(); buffers[i] = id();
            names.add(name(block, i == 0 ? "_dm_visible_worlds" : "_dm_visible_ids"));
            types.add(op(29, array, elements[i])); // OpTypeRuntimeArray
            types.add(op(30, block, array)); // OpTypeStruct
            // Uniform + BufferBlock is the Vulkan 1.0 storage-buffer encoding.
            types.add(op(OP_TYPE_POINTER, pointer, UNIFORM, block));
            variables.add(op(OP_VARIABLE, pointer, buffers[i], UNIFORM));
            annotations.add(op(OP_DECORATE, array, 6, strides[i])); // ArrayStride
            annotations.add(op(OP_DECORATE, block, 3)); // BufferBlock
            annotations.add(op(OP_MEMBER_DECORATE, block, 0, 35, 0)); // Offset
            annotations.add(op(OP_MEMBER_DECORATE, block, 0, 24)); // NonWritable
            annotations.add(op(OP_DECORATE, buffers[i], 34, 3)); // DescriptorSet
            annotations.add(op(OP_DECORATE, buffers[i], 33, i)); // Binding
            if (i == 0) {
                annotations.add(op(OP_MEMBER_DECORATE, block, 0, 5)); // ColMajor
                annotations.add(op(OP_MEMBER_DECORATE, block, 0, 7, 16)); // MatrixStride
            }
        }
        int uintPointer = id(), worldPointer = id();
        types.add(op(OP_TYPE_POINTER, uintPointer, UNIFORM, uintType));
        types.add(op(OP_TYPE_POINTER, worldPointer, UNIFORM, matrix));
        int instanceType = intType;
        if (!newInstance) {
            for (int[] words : instructions)
                if ((words[0] & 0xffff) == OP_VARIABLE && words[2] == instance)
                    instanceType = pointers.get(words[1])[3];
        }
        int vertexInstance = id(), idPointer = id(), visible = id(), transformPointer = id(), world = id();
        int[][] load = {
            op(OP_LOAD, instanceType, vertexInstance, instance),
            op(OP_ACCESS_CHAIN, uintPointer, idPointer, buffers[1], zero, vertexInstance),
            op(OP_LOAD, uintType, visible, idPointer),
            op(OP_ACCESS_CHAIN, worldPointer, transformPointer, buffers[0], zero, visible),
            op(OP_LOAD, matrix, world, transformPointer),
            op(OP_STORE, inputId, world)
        };
        ArrayList<int[]> globals = new ArrayList<>(), functions = new ArrayList<>();
        HashSet<Integer> privateIds = new HashSet<>(); privateIds.add(inputId);
        boolean inFunctions = false, inEntry = false, pendingLoad = false, entryLabelSeen = false;
        for (int[] words : instructions) {
            int opcode = words[0] & 0xffff;
            if (opcode == OP_FUNCTION) { inFunctions = true; inEntry = words[2] == entry; }
            if (!inFunctions) {
                if (opcode == OP_ENTRY_POINT) {
                    // The null-terminated entry-point name precedes interface IDs.
                    int start = 3;
                    while (true) {
                        int word = words[start++];
                        if ((word & 0xff)==0 || (word & 0xff00)==0 || (word & 0xff0000)==0 || (word & 0xff000000)==0) break;
                    }
                    ArrayList<Integer> operands = new ArrayList<>();
                    for (int j = 1; j < words.length; ++j)
                        if (j < start || words[j] != inputId) operands.add(words[j]);
                    if (newInstance) operands.add(instance);
                    words = op(OP_ENTRY_POINT, operands.stream().mapToInt(Integer::intValue).toArray());
                }
                if (opcode == OP_DECORATE && words[1] == inputId && words[2] != 0) continue;
                if (opcode == OP_VARIABLE) {
                    if (words[2] == inputId) { words[1] = matrixPointer; words[3] = PRIVATE; }
                    variables.add(words);
                } else globals.add(words);
                continue;
            }
            if (inEntry && opcode == OP_LABEL && !entryLabelSeen) { entryLabelSeen = true; pendingLoad = true; functions.add(words); continue; }
            if (pendingLoad && opcode != OP_VARIABLE && opcode != 8 && opcode != 317) {
                for (int[] instruction : load) functions.add(instruction);
                pendingLoad = false;
            }
            if ((opcode == OP_ACCESS_CHAIN || opcode == OP_IN_BOUNDS_ACCESS_CHAIN || opcode == OP_COPY_OBJECT) && privateIds.contains(words[3])) {
                words[1] = privatePointer(pointers.get(words[1])[3]);
                privateIds.add(words[2]);
            }
            functions.add(words);
        }
        ArrayList<int[]> result = new ArrayList<>();
        boolean addedAnnotations = false, addedNames = false;
        for (int[] words : globals) {
            int opcode = words[0] & 0xffff;
            if (!addedNames && (opcode == OP_DECORATE || opcode == OP_MEMBER_DECORATE || (opcode >= 19 && opcode <= 52))) {
                result.addAll(names); addedNames = true;
            }
            if (!addedAnnotations && opcode >= 19 && opcode <= 52) {
                result.addAll(annotations); addedAnnotations = true;
            }
            result.add(words);
        }
        result.addAll(types); result.addAll(variables); result.addAll(functions);
        header[3] = nextId;
        int size = 5;
        for (int[] words : result) size += words.length;
        ByteBuffer output = ByteBuffer.allocate(size * 4).order(ByteOrder.LITTLE_ENDIAN);
        for (int word : header) output.putInt(word);
        for (int[] words : result) for (int word : words) output.putInt(word);
        return output.array();
    }
}
