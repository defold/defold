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

package com.defold.editor.luart;

import clojure.lang.IFn;
import clojure.lang.RT;
import org.luaj.vm2.Varargs;
import org.luaj.vm2.lib.VarArgFunction;

public class DefoldVarArgFn extends VarArgFunction {

    private final IFn fn;

    public DefoldVarArgFn(IFn fn) {
        this.fn = fn;
    }

    @Override
    public Varargs invoke(Varargs args) {
        int n = args.narg();
        Object[] array = new Object[n];
        for (int i = 0; i < n; i++) {
            array[i] = args.arg(i + 1);
        }
        return (Varargs) fn.applyTo(RT.seq(array));
    }
}
