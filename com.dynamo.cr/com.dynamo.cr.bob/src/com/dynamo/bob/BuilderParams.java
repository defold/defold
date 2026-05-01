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

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Builder parameters
 * @author Christian Murray
 *
 */
@Retention(RetentionPolicy.RUNTIME)
@Target(ElementType.TYPE)
public @interface BuilderParams {
    /**
     * Get builder name
     * @return builder name
     */
    String name();

    /**
     * Get output extension
     * @return output extension
     */
    String outExt();

    /**
     * Get input extension(s)
     * @return input extension(s)
     */
    String[] inExts();

    /**
     * Get bool that shows if tasks can be cached
     * @return if task should be cached
     */
    boolean isCacheble() default false;

    /**
     * Get parameters that should be included in the task signature produced by the builder
     * @return Bob parameters that may affect the builder's task signature
     */
    String[] paramsForSignature() default {};
}
