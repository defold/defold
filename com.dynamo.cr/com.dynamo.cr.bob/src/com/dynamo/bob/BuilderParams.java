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
