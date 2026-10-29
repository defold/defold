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

/** Test-only entry points used to exercise the shared command dispatcher. */
public final class DispatchTestCommands {
    public static final class Echo {
        public static void main(String[] args) {
            System.out.println(args.length);
            for (int i = 0; i < args.length; ++i)
                System.out.println(i + ":" + args[i]);
        }
    }

    public static final class Fail {
        public static void main(String[] args) {
            throw new IllegalStateException("dispatch-test-failure");
        }
    }

    public static final class NoMain {}

    public static final class InstanceMain {
        public void main(String[] args) {}
    }

    public static final class WrongReturnType {
        public static int main(String[] args) {
            return 0;
        }
    }
}
