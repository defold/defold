// Copyright 2020-2025 The Defold Foundation
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
import org.luaj.vm2.LuaValue;
import org.luaj.vm2.Varargs;
import org.luaj.vm2.lib.LibFunction;

public class DefoldFourArgsLuaFn extends LibFunction {
    private final IFn fn;

    public DefoldFourArgsLuaFn(IFn fn) {
        this.fn = fn;
    }

    @Override
    public LuaValue call() {
        return (LuaValue) fn.invoke(NIL, NIL, NIL, NIL);
    }

    @Override
    public LuaValue call(LuaValue a) {
        return (LuaValue) fn.invoke(a, NIL, NIL, NIL);
    }

    @Override
    public LuaValue call(LuaValue a, LuaValue b) {
        return (LuaValue) fn.invoke(a, b, NIL, NIL);
    }

    @Override
    public LuaValue call(LuaValue a, LuaValue b, LuaValue c) {
        return (LuaValue) fn.invoke(a, b, c, NIL);
    }

    @Override
    public LuaValue call(LuaValue a, LuaValue b, LuaValue c, LuaValue d) {
        return (LuaValue) fn.invoke(a, b, c, d);
    }

    @Override
    public Varargs invoke(Varargs args) {
        return (LuaValue) fn.invoke(args.arg1(), args.arg(2), args.arg(3), args.arg(4));
    }
}
