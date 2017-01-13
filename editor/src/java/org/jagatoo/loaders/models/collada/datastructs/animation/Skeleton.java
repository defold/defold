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
import java.util.HashMap;
import java.util.Iterator;

import org.openmali.vecmath2.Matrix3f;
import org.openmali.vecmath2.Point3f;

/**
 * A Skeleton. It contains a root bone, which can have several children
 * 
 * @author Amos Wenger (aka BlueSky)
 * @author Matias Leone (aka Maguila)
 */
public class Skeleton implements Iterable<Bone>
{
    /** The root bone : there's only one (for simple implementation) */
    private final Bone rootBone;
    
    private final HashMap<String, Bone> boneMap = new HashMap<String, Bone>();
    
    // GC-friendly hacks
    private final Matrix3f tempMatrix = new Matrix3f();
    private final Point3f tempPoint = new Point3f();
    
    /**
     * The position of our Skeleton. The root bone is transformed by this
     * position (and thus, the children bones are transformed too).
     */
    public final Point3f relativeTranslation;
    
    /** The keyframes of position */
    public ArrayList<KeyFrameTuple3f> transKeyFrames = new ArrayList<KeyFrameTuple3f>();;
    
    /**
     * Iterator for easy managment of the bones
     */
    private SkeletonIterator iterator = new SkeletonIterator(this);
    
    
    /**
     * @return the rootBone
     */
    public final Bone getRootBone()
    {
        return ( rootBone );
    }
    
    public final Bone getBoneBySourceId( String sourceId )
    {
        return ( boneMap.get( sourceId ) );
    }
    
    /**
     * Selects the current translation key frame, based on the current time.
     * 
     * @param currentTime
     *                beetween 0 and the end of the animation, in milliseconds
     * @return frame index selected
     */
    public int selectCurrentTransFrame( long currentTime )
    {
        return ( KeyFrame.searchNextFrame( transKeyFrames, currentTime ) );
    }
    
    /**
     * Updates a bone, e.g. compute its absolute rotation/translation from its
     * relative ones.
     * 
     * @param parentTrans
     * @param bone
     */
    private void updateBone( Bone parentBone, Bone bone )
    {
        if ( parentBone == null )
        {
            /*
             * Root bone
             */
            
            bone.absoluteRotation.set( bone.getBindRotation() );
            bone.absoluteRotation.mul( bone.relativeRotation );
            
            bone.absoluteTranslation.set( relativeTranslation );
            //bone.absoluteTranslation.addZ( bone.getLength() );
            bone.absoluteTranslation.normalize();
            bone.absoluteTranslation.mul( bone.getLength() );
            
            bone.absoluteScaling.set( bone.relativeScaling );
            
            bone.absoluteTransformation.setIdentity();
        }
        else
        {
            /*
             * Child bone
             */
            
            bone.absoluteRotation.set( parentBone.absoluteRotation );
            bone.absoluteRotation.mul( bone.getBindRotation() );
            bone.absoluteRotation.mul( bone.relativeRotation );
            
            bone.absoluteTranslation.set( parentBone.absoluteTranslation );
            tempMatrix.set( bone.absoluteRotation );
            tempPoint.set( 0f, 0f, bone.getLength() );
            tempMatrix.transform( tempPoint );
            bone.absoluteTranslation.add( tempPoint );
            
            bone.absoluteScaling.set( parentBone.absoluteScaling );
            bone.absoluteScaling.set( bone.absoluteScaling.getX() *
                                      bone.relativeScaling.getX(), bone.absoluteScaling.getY() *
                                      bone.relativeScaling.getY(), bone.absoluteScaling.getZ() *
                                      bone.relativeScaling.getZ()
                                    );
            
            bone.absoluteTransformation.set( parentBone.absoluteTransformation );
            bone.absoluteTransformation.mul( bone.bindMatrix );
        }
        
        // Both for root and children nodes
        bone.absoluteTranslation.set( bone.absoluteTranslation.getX() *
                                      bone.absoluteScaling.getX(), bone.absoluteTranslation.getY() *
                                      bone.absoluteScaling.getY(), bone.absoluteTranslation.getZ() *
                                      bone.absoluteScaling.getZ()
                                    );
        
        for ( int i = 0; i < bone.numChildren(); i++ )
        {
            updateBone( bone, bone.getChild( i ) );
        }
    }
    
    /**
     * Updates the skeleton.<br>
     * It recomputes the transform matrix of each bone,
     * starting with the root bone and then each child recursively.
     */
    public void updateAbsolutes()
    {
        updateBone( null, rootBone );
    }
    
    /**
     * {@inheritDoc}
     */
    public Iterator<Bone> iterator()
    {
    	if ( this.iterator == null )
    	{
    		this.iterator = new SkeletonIterator( this );
    	}
    	
    	if ( !this.iterator.hasNext() )
    	{
    		resetIterator();
    	}
    	
        return ( this.iterator );
    }
    
    public void resetIterator()
    {
    	this.iterator = new SkeletonIterator( this );
    	this.iterator.reset();
    }
    
    /**
     * Creates a new Skeleton.
     * 
     * @param rootBone
     *                The root bone
     * @param relativeTranslation
     *                The position of the Skeleton
     * @param boneList
     */
    public Skeleton( Bone rootBone, Point3f relativeTranslation, ArrayList<Bone> boneList )
    {
        this.rootBone = rootBone;
        
        for ( int i = 0; i < boneList.size(); i++ )
        {
            final Bone bone = boneList.get( i );
            
            this.boneMap.put( bone.getSourceId(), bone );
        }
        
        this.relativeTranslation = relativeTranslation;
        
        this.transKeyFrames = new ArrayList<KeyFrameTuple3f>();
    }
}
