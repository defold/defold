package com.dynamo.cr.contenteditor.scene;

import javax.vecmath.Matrix4d;

import com.dynamo.cr.contenteditor.math.Transform;

public class NodeUtil
{
    public static void setWorldTransform(Node node, Matrix4d transform)
    {
        // TODO: Verkar buggig. Funkade inte med MoveManipulator2 iallafall
        // När man ändrade till setPosition funkade allt fint...
        // Rotera ett object 120 runt y och flytta sedan. Objektet började rotera.
        // Kan det ha något med euler att göra?

        Matrix4d world_inv = new Matrix4d();
        node.getTransform(world_inv);
        world_inv.invert();

        transform.mul(world_inv, transform);
        node.transformLocal(transform);
    }

    public static void setWorldTransform(Node node, Transform transform)
    {
        Transform world_inv = new Transform();
        node.getTransform(world_inv);
        world_inv.invert();

        transform.mul(world_inv, transform);
        node.transformLocal(transform);
    }

}
