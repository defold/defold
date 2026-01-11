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

package com.dynamo.bob;

import com.dynamo.bob.fs.IResource;


/**
 * Compile exception
 * @author Christian Murray
 *
 */
public class CompileExceptionError extends Exception {
    private static final long serialVersionUID = -3189379067765141096L;

    private IResource resource;
    private int lineNumber;

    public CompileExceptionError(IResource resource, int lineNumber, String message) {
        super(message);
        this.resource = resource;
        this.lineNumber = lineNumber;
    }

    public CompileExceptionError(IResource resource, int lineNumber, Exception e) {
        super(e.getMessage());
        this.resource = resource;
        this.lineNumber = lineNumber;
    }

    public CompileExceptionError(String message, Throwable e) {
        super(message, e);
        this.resource = null;
        this.lineNumber = -1;
    }

    public CompileExceptionError(Throwable e) {
        super(e);
        this.resource = null;
        this.lineNumber = -1;
    }

    public CompileExceptionError(String message) {
        super(message);
        this.resource = null;
        this.lineNumber = -1;
    }

    public CompileExceptionError(IResource resource, int lineNumber, String message, Throwable e) {
        super(message, e);
        this.resource = resource;
        this.lineNumber = lineNumber;
    }

    public IResource getResource() {
        return resource;
    }

    public int getLineNumber() {
        return lineNumber;
    }
}
