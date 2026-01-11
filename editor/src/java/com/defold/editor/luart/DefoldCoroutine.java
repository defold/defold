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

import clojure.lang.Var;
import clojure.lang.Volatile;
import org.luaj.vm2.*;
import org.luaj.vm2.lib.OneArgFunction;
import org.luaj.vm2.lib.VarArgFunction;

public class DefoldCoroutine {

    /*
    This class defines the necessary thread binding conveyance support for Lua
    coroutines that run in background threads. From the point of view of the
    editor, editor threads create a coroutine (it doesn't start running yet),
    and then continuously resume while it hasn't finished. From the point of
    view of the coroutine, there is a single coroutine thread per coroutine
    that starts running on first resume, and then it gets blocked when the
    coroutine's lua function yields control back to the editor, continuing
    execution after the later resume call by the editor thread.

    Binding conveyance works like this:
    1. Right before the resume call by the editor thread, we store current
       thread bindings in a volatile of the coroutine object (see
       DefoldLuaThread::resume)
    2. The coroutine thread invokes a lua function to start the coroutine
       execution. Before invoking the function, we restore the thread bindings
       that were set by the editor thread when it called resume. This is
       implemented by wrapping the Lua function we want to execute with the one
       that restores the thread bindings before calling the wrapped function
       (this is implemented in Create class).
    3. If the coroutine thread yields, the coroutine thread will block until the
       editor thread resumes it again. When the editor thread resumes it again,
       it might have other thread bindings, so they get updated in the resume
       call. After it calls resume, the coroutine thread that was waiting in the
       yield call wakes up, and at that point we want to reset the thread
       bindings of the coroutine thread again. This is implemented in
       Yield::invoke method, after doing the actual yield machinery.
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
