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

import clojure.lang.Var;
import clojure.lang.Volatile;
import org.luaj.vm2.*;
import org.luaj.vm2.lib.OneArgFunction;
import org.luaj.vm2.lib.VarArgFunction;

public class DefoldCoroutine {

    /*
    This class defines the necessary thread binding conveyance support for Lua
    coroutines that run in background threads:
    1. Calling coroutine.resume() invokes the DefoldLuaThread::resume method
       from a Clojure thread. At this point we memoize the current thread
       bindings.
    2. Calling coroutine.create() invokes Create::call method. In the method,
       we create our own implementation of the LuaThread (i.e. coroutine) that
       restores the thread binding before beginning to execute the actual
       function code.
    3. Finally, when the coroutine is suspended using coroutine.yield(), the
       coroutine thread blocks until it receives a response. It receives a
       response when another Clojure thread calls coroutine.resume(), possibly
       with different thread bindings! At this point we should also refresh the
       coroutine thread's bindings. This is implemented in a Yield::invoke
       method after doing the actual yield machinery.
     */

    public static class DefoldLuaThread extends LuaThread {
        public final Volatile threadBindings;

        public DefoldLuaThread(Volatile threadBindings, Globals globals, LuaValue func) {
            super(globals, func);
            this.threadBindings = threadBindings;
        }

        @Override
        public Varargs resume(Varargs args) {
            threadBindings.reset(Var.cloneThreadBindingFrame());
            return super.resume(args);
        }
    }

    public static class Create extends OneArgFunction {
        private final Globals globals;

        public Create(Globals globals) {
            this.globals = globals;
        }

        @Override
        public LuaValue call(LuaValue arg) {
            LuaFunction func = arg.checkfunction();
            Volatile threadBindings = new Volatile(null);
            VarArgFunction threadBoundFunc = new VarArgFunction() {
                @Override
                public Varargs invoke(Varargs args) {
                    Var.resetThreadBindingFrame(threadBindings.deref());
                    return func.invoke(args);
                }
            };
            return new DefoldLuaThread(threadBindings, globals, threadBoundFunc);
        }
    }

    public static class Yield extends VarArgFunction {

        private final Globals globals;

        public Yield(Globals globals) {
            this.globals = globals;
        }

        @Override
        public Varargs invoke(Varargs args) {
            Varargs result = globals.yield(args);
            Var.resetThreadBindingFrame(((DefoldLuaThread) globals.running).threadBindings.deref());
            return result;
        }
    }
}
