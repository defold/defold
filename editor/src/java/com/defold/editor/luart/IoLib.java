// Copyright 2020 The Defold Foundation
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

import org.luaj.vm2.lib.jse.JseIoLib;

import java.io.IOException;

public class IoLib extends JseIoLib {

    /**
     * made public to avoid reflection from clojure
     */
    @Override
    public File openFile(String s, boolean b, boolean b1, boolean b2, boolean b3) throws IOException {
        return super.openFile(s, b, b1, b2, b3);
    }
}
