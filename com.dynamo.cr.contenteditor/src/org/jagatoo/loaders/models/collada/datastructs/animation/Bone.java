/**
 * Copyright (c) 2007-2009, JAGaToo Project Group all rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * Neither the name of the 'Xith3D Project Group' nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) A
 * RISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE
 */
package org.jagatoo.loaders.models.collada.datastructs.animation;

import java.util.ArrayList;

import org.openmali.vecmath2.Matrix4f;
import org.openmali.vecmath2.Quaternion4f;
import org.openmali.vecmath2.Tuple3f;
import org.openmali.vecmath2.Vector3f;
import org.openmali.vecmath2.util.FloatUtils;

/**
 * A Bone (of a Skeleton)
 * 
 * @author Amos Wenger (aka BlueSky)
 * @author Matias Leone (aka Maguila)
 */
public class Bone
{
    /** The source-id of this bone */
    private final String sid;
    
    /** The name of this bone */
    private final String name;
    
    /** The length of this bone */
    private float length;
    
    /** The bind rotation */
    private final Quaternion4f bindRotation;
    
    /** The bind matrix */
    public final Matrix4f bindMatrix;
    
    /** The inverse bind matrix */
    public final Matrix4f invBindMatrix;
    
    /** The rotation of this bone */
    public final Quaternion4f relativeRotation;
    
    /**
     * The scaling of this bone, along the three axis : X, Y, and Z.
     */
    public final Tuple3f relativeScaling;
    
    /**
     * The absolute rotation of this bone, ie, a multiplication of all the
     * velocities from the rootBone to this bone, passing by its parents.
     *
     * Stupid question #1 : Why isn't it automatically updated when I
     * setRotation() ? Intelligent answer #1 : For efficiency. If you set the
     * rotation of several bones in a skeleton, you want it only to recompute
     * the transform matrix for each bone once per frame, not once per
     * setRotation()
     */
    Quaternion4f absoluteRotation;
    
    /**
     * The absolute translation of this bone, ie, the position of this bone...
     *
     * Stupid question #1 bis : Why isn't it automatically updated when I
     * setRotation() ? Intelligent answer #1 bis : For efficiency. If you set
     * the rotation of several bones in a skeleton, you want it only to
     * recompute the transform matrix for each bone once per frame, not once per
     * setRotation()
     */
    Vector3f absoluteTranslation;
    
    /**
     * The absolute scaling of this bone, along the three axis : X, Y, and Z.
     */
    Tuple3f absoluteScaling;
    
    Matrix4f absoluteTransformation;
    
    
    /**
     * Temporal key frames for the bone. They are reference extracted from the
     * current animation and they will change every time you play a different
     * animation. They are here to simplify the animation algorithm.
     * These are ROTATION key frames.
     */
    public ArrayList<KeyFrameQuat4f> rotKeyFrames = new ArrayList<KeyFrameQuat4f>();
    
    /**
     * Temporal key frames for the bone. They are reference extracted from the
     * current animation and they will change every time you play a different
     * animation. They are here to simplify the animation algorithm.
     * These are SCALE key frames.
     */
    public ArrayList<KeyFrameTuple3f> scaleKeyFrames = new ArrayList<KeyFrameTuple3f>();
    
    /** Children (optional) */
    private ArrayList<Bone> children;
    
    public final String getSourceId()
    {
        return ( sid );
    }
    
    /**
     * @return the name of this bone.
     */
    public final String getName() 
    {
        return ( name );
    }
    
    /**
     * Sets the length.
     * 
     * @param length
     */
    public void setLength( float length )
    {
        this.length = length;
        
        bindMatrix.m23( length );
        invBindMatrix.invert( bindMatrix );
    }
    
    /**
     * @return the length.
     */
    public final float getLength()
    {
        return ( length );
    }
    
    /**
     * Sets the bindRotation.
     * 
     * @param quat
     */
    public void setBindRotation( Quaternion4f quat )
    {
        bindRotation.set( quat );
        bindMatrix.set( bindRotation );
    }
    
    /**
     * @return the bindRotation
     */
    public final Quaternion4f getBindRotation()
    {
        return ( bindRotation );
    }
    
    /**
     * Get the absolute translation of this bone. Absolute translation/rotation
     * are updated by the Skeleton.updateAbsolutes() method.
     *
     * @return a point3f containing the absolute translation
     */
    public final Vector3f getAbsoluteTranslation()
    {
        return ( absoluteTranslation );
    }
    
    /**
     * Get the absolute rotation of this bone. Absolute translation/rotation are
     * updated by the Skeleton.updateAbsolutes() method.
     *
     * @return a quaternion containing the absolute rotation
     */
    public final Quaternion4f getAbsoluteRotation()
    {
        return ( absoluteRotation );
    }
    
    public final Tuple3f getAbsoluteScaling()
    {
        return ( absoluteScaling );
    }
    
    public final Matrix4f getAbsoluteTransformation()
    {
        return ( absoluteTransformation );
    }
    
    /**
     * @return true if the bone has at least one key frame of any kind.
     */
    public final boolean hasKeyFrames()
    {
    	int total = 0;
    	
    	if ( rotKeyFrames != null )
    		total += rotKeyFrames.size();
    	
    	if ( scaleKeyFrames != null )
            total += scaleKeyFrames.size();
    	
        return ( total > 0 );
    }
    
    /**
     * Completes the relativeTranslation and relativeRotation all with 0.
     */
    public void setNoRelativeMovement()
    {
        this.relativeRotation.set( 0f, 0f, 0f, 1f );
        this.relativeScaling.set( 1f, 1f, 1f );
    }
    
    /**
     * Selects the current translation key frame, based on the current time
     * 
     * @param currentTime
     *                beetween 0 and the end of the animation, in milliseconds
     * @return frame index selected
     */
    public int selectCurrentRotFrame( long currentTime )
    {
        return ( KeyFrame.searchNextFrame( rotKeyFrames, currentTime ) );
    }
    
    /**
     * Selects the current scaling key frame, based on the current time
     * 
     * @param currentTime
     *                beetween 0 and the end of the animation, in miliseconds
     * @return frame index selected
     */
    public int selectCurrentScaleFrame( long currentTime )
    {
        return ( KeyFrame.searchNextFrame( scaleKeyFrames, currentTime ) );
    }
    
    /**
     * Adds a child bone.
     * 
     * @param bone
     */
    public void addChild( Bone bone )
    {
        if ( children == null )
        {
            children = new ArrayList<Bone>();
        }
        
        children.add( bone );
    }
    
    /**
     * Removes a child bone.
     * 
     * @param bone
     */
    public void removeChild( Bone bone )
    {
        if ( children != null )
        {
            children.remove( bone );
        }
    }
    
    /**
     * @return the number of children of this bone.
     */
    public final int numChildren()
    {
        return ( ( children == null ) ? 0 : children.size() );
    }
    
    /**
     * Gets a bone by index.
     * 
     * @param i
     *                The index of the bone you want to get.
     * @return The bone :)
     */
    public final Bone getChild( int i )
    {
        return ( ( children == null ) ? null : children.get( i ) );
    }
    
    @Override
    public String toString()
    {
        return ( name + "] Bind rotation : " + bindRotation + ", length : " + length );
    }
    
    /**
     * Create a new Bone
     * 
     * @param sid
     * @param name
     *                The name of this bone
     * @param length
     *                The length of this bone
     * @param bindRotation
     *                The bind rotation of this bone
     */
    public Bone( String sid, String name, Matrix4f matrix, Quaternion4f bindRotation )
    {
        this.sid = sid;
        this.name = name;
        
        this.bindMatrix = matrix;
        this.invBindMatrix = new Matrix4f();
        invBindMatrix.invert( bindMatrix );
        
        this.length = FloatUtils.vectorLength( matrix.m03(), matrix.m13(), matrix.m23() );
        this.bindRotation = bindRotation;
        
        // Init relative rot/scaling
        this.relativeRotation = new Quaternion4f( 0f, 0f, 0f, 1f );
        this.relativeScaling = new Tuple3f( 1f, 1f, 1f );
        
        // Init absolute rot/trans/scaling
        this.absoluteRotation = new Quaternion4f( 0f, 0f, 0f, 1f );
        this.absoluteTranslation = new Vector3f( 0f, 0f, 0f );
        this.absoluteScaling = new Tuple3f( 1f, 1f, 1f );
        
        this.absoluteTransformation = new Matrix4f( bindMatrix );
    }
}
