// Copyright 2020 The Defold Foundation
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
     * Task creating order. A task with create-order X
     * is guaranteed to be created after every task with create-order Y
     * and where X > Y
     * The create-order can be used for task that collect inputs from
     * task outputs
     * @return create order
     */
    int createOrder() default 0;
}
