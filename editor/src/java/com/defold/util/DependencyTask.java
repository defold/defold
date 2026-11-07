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

package com.defold.util;

import clojure.lang.IFn;
import java.util.concurrent.CountedCompleter;
import java.util.concurrent.atomic.AtomicReference;

public final class DependencyTask extends CountedCompleter<Void> {
    private static final long serialVersionUID = 1L;

    private final IFn operation;
    private final AtomicReference<Throwable> failure;
    private final Object endpoints;

    public DependencyTask(IFn operation, AtomicReference<Throwable> failure, Object endpoints) {
        this.operation = operation;
        this.failure = failure;
        this.endpoints = endpoints;
    }

    private DependencyTask(DependencyTask parent, Object endpoints) {
        super(parent);
        this.operation = parent.operation;
        this.failure = parent.failure;
        this.endpoints = endpoints;
    }

    public void forkChild(Object endpoints) {
        DependencyTask child = new DependencyTask(this, endpoints);
        addToPendingCount(1);
        child.fork();
    }

    @Override
    public void compute() {
        try {
            operation.invoke(this, endpoints);
        } catch (Throwable error) {
            failure.compareAndSet(null, error);
        }
        // Finish all spawned tasks before reporting a failure.
        tryComplete();
    }
}
