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

package com.defold.engine;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.Arrays;

/** Loads the engine modules exposed through dmengine_shared. */
public final class EngineJni {
    static {
        // An unpacked editor library can be supplied by absolute path.
        String libraryPath = System.getProperty("dmengine.library");
        if (libraryPath != null)
            System.load(libraryPath);
        else
            System.loadLibrary("dmengine_shared");
    }

    private EngineJni() {}

    /** Ensures the library is loaded and its module bindings are registered. */
    public static void Initialize() {}

    /** Invokes a facade's public static main(String[]) in com.defold.engine. */
    public static void main(String[] args) throws Throwable {
        if (args.length == 0 || args[0].equals("--help")) {
            System.out.println("Usage: java -jar dmengine.jar <class-name> [arguments]");
            System.out.println("The class must provide public static void main(String[]) in com.defold.engine.");
            return;
        }
        Class<?> module = Class.forName(EngineJni.class.getPackage().getName() + "." + args[0]);
        if (module == EngineJni.class)
            throw new IllegalArgumentException("EngineJni cannot dispatch to itself");
        Method main = module.getMethod("main", String[].class);
        if (!Modifier.isStatic(main.getModifiers()) || main.getReturnType() != void.class)
            throw new IllegalArgumentException("Entry point must be public static void main(String[])");
        try {
            main.invoke(null, (Object)Arrays.copyOfRange(args, 1, args.length));
        } catch (InvocationTargetException e) {
            throw e.getCause();
        }
    }
}
