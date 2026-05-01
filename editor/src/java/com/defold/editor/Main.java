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

package com.defold.editor;

import java.io.IOException;

/**
 * Using separate Main class is needed to avoid error "JavaFX runtime components
 * are missing, and are required to run this application."
 *
 * If class with main function extends {@link javafx.application.Application}
 * class, Java 11 checks if the module javafx.graphics is available and refuses
 * to start otherwise. Using other class as container for main is a workaround
 *
 * @see <a href="https://github.com/javafxports/openjdk-jfx/issues/236">Issue
 * discussion on github</a>
 */
public class Main {
    public static void main(String[] args) throws IOException {
        Start.main(args);
    }
}
