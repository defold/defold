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
import org.luaj.vm2.LuaFunction;
import org.luaj.vm2.LuaString;
import org.luaj.vm2.lib.jse.JseIoLib;

import java.io.IOException;

public class DefoldIoLib extends JseIoLib {

    private final IFn resolveFileName;
    private final LuaFunction onWrittenFileClose;

    public DefoldIoLib(IFn resolveFileName, LuaFunction onWrittenFileClose) {
        this.resolveFileName = resolveFileName;
        this.onWrittenFileClose = onWrittenFileClose;
    }

    @Override
    protected File openFile(String filename, boolean readMode, boolean appendMode, boolean updateMode, boolean binaryMode) throws IOException {
        File result = super.openFile((String) resolveFileName.invoke(filename), readMode, appendMode, updateMode, binaryMode);
        if (readMode || onWrittenFileClose == null) {
            return result;
        } else {
            return new CallbackOnCloseFile(result);
        }
    }

    private class CallbackOnCloseFile extends File {

        private boolean written;
        private final File wrappedFile;

        public CallbackOnCloseFile(File wrappedFile) {
            this.wrappedFile = wrappedFile;
        }

        @Override
        public void write(LuaString string) throws IOException {
            wrappedFile.write(string);
            written = true;
        }

        @Override
        public void flush() throws IOException {
            wrappedFile.flush();
        }

        @Override
        public boolean isstdfile() {
            return wrappedFile.isstdfile();
        }

        @Override
        public void close() throws IOException {
            wrappedFile.close();
            if (written) {
                written = false;
                onWrittenFileClose.invoke();
            }
        }

        @Override
        public boolean isclosed() {
            return wrappedFile.isclosed();
        }

        @Override
        public int seek(String option, int bytecount) throws IOException {
            return wrappedFile.seek(option, bytecount);
        }

        @Override
        public void setvbuf(String mode, int size) {
            wrappedFile.setvbuf(mode, size);
        }

        @Override
        public int remaining() throws IOException {
            return wrappedFile.remaining();
        }

        @Override
        public int peek() throws IOException {
            return wrappedFile.peek();
        }

        @Override
        public int read() throws IOException {
            return wrappedFile.read();
        }

        @Override
        public int read(byte[] bytes, int offset, int length) throws IOException {
            return wrappedFile.read(bytes, offset, length);
        }
    }
}
