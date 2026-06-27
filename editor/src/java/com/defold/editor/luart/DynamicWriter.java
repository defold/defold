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

package com.defold.editor.luart;

import clojure.lang.IFn;

import java.io.IOException;
import java.io.Writer;

public final class DynamicWriter extends Writer {

    private final IFn writerFn;

    public DynamicWriter(IFn writerFn) {
        this.writerFn = writerFn;
    }

    @Override
    public void write(char[] cbuf, int off, int len) throws IOException {
        ((Writer) writerFn.invoke()).write(cbuf, off, len);
    }

    @Override
    public void flush() throws IOException {
        ((Writer) writerFn.invoke()).flush();
    }

    @Override
    public void close() throws IOException {
        ((Writer) writerFn.invoke()).flush();
    }
}
