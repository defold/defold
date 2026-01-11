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

import org.luaj.vm2.Globals;
import org.luaj.vm2.Varargs;
import org.luaj.vm2.lib.VarArgFunction;

import java.io.InputStream;

/**
 * This is a copy of {@link org.luaj.vm2.lib.PackageLib.searchpath}, the only
 * change is default path separator is always "/" instead of being
 * platform-dependent, since we use unix-style path separators in resources
 */
public class SearchPath extends VarArgFunction {
    private final Globals globals;

    public SearchPath(Globals globals) {
        this.globals = globals;
    }


    public Varargs invoke(Varargs args) {
        String name = args.checkjstring(1);
        String path = args.checkjstring(2);
        String sep = args.optjstring(3, ".");
        String rep = args.optjstring(4, "/");

        // check the path elements
        int e = -1;
        int n = path.length();
        StringBuffer sb = null;
        name = name.replace(sep.charAt(0), rep.charAt(0));
        while ( e < n ) {

            // find next template
            int b = e+1;
            e = path.indexOf(';',b);
            if ( e < 0 )
                e = path.length();
            String template = path.substring(b,e);

            // create filename
            int q = template.indexOf('?');
            String filename = template;
            if ( q >= 0 ) {
                filename = template.substring(0,q) + name + template.substring(q+1);
            }

            // try opening the file
            InputStream is = globals.finder.findResource(filename);
            if (is != null) {
                try { is.close(); } catch ( java.io.IOException ignored) {}
                return valueOf(filename);
            }

            // report error
            if ( sb == null )
                sb = new StringBuffer();
            sb.append("\n\t").append(filename);
        }
        return varargsOf(NIL, valueOf(sb.toString()));
    }
}
