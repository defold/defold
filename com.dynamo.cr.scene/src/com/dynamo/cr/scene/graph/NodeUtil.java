package com.dynamo.cr.scene.graph;

import javax.vecmath.Matrix4d;

import com.dynamo.cr.scene.math.Transform;

public class NodeUtil
{
    public static void setWorldTransform(Node node, Matrix4d transform)
    {
        // TODO: Verkar buggig. Funkade inte med MoveManipulator2 iallafall
        // N�r man �ndrade till setPosition funkade allt fint...
        // Rotera ett object 120 runt y och flytta sedan. Objektet b�rjade rotera.
        // Kan det ha n�got med euler att g�ra?

        Matrix4d world_inv = new Matrix4d();
        node.getWorldTransform(world_inv);
        world_inv.invert();

        transform.mul(world_inv, transform);
        node.transformLocal(transform);
    }

    public static void setWorldTransform(Node node, Transform transform)
    {
        Transform world_inv = new Transform();
        node.getWorldTransform(world_inv);
        world_inv.invert();

        transform.mul(world_inv, transform);
        node.transformLocal(transform);
    }

}
