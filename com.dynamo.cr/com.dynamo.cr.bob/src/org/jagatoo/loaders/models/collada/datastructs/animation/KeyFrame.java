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

import java.util.List;

import org.jagatoo.loaders.models.collada.Rotations;
import org.openmali.FastMath;
import org.openmali.vecmath2.Tuple3f;

/**
 * A KeyFrame contains information for the animation of a Bone. It can contain
 * translation, rotation or scale information.
 * 
 * @author Amos Wenger (aka BlueSky)
 * @author Matias Leone (aka Maguila)
 */
public abstract class KeyFrame
{
    /**
     * An Axis.
     * 
     * @author Amos Wenger (aka BlueSky)
     */
    public static enum Axis
    {
        /** X Axis : (1, 0, 0) */
        X,
        /** Y Axis : (0, 1, 0) */
        Y,
        /** Z Axis : (0, 0, 1) */
        Z
    }
    
    /**
     * Key frame time
     */
    public long time;
    
    /**
     * Creates a translation key frame
     * 
     * @param time
     *                frame time
     * @param values
     *                float values of the translation
     * @param valueIndex
     *                first value index
     * @return a new key frame
     */
    public static KeyFrame buildPoint3fKeyFrame( float time, float[] values, int valueIndex )
    {
        KeyFrameTuple3f frame = new KeyFrameTuple3f();
        frame.time = (long)( time * 1000f );
        
        frame.setValue( new Tuple3f(
                values[valueIndex],
                values[valueIndex + 1],
                values[valueIndex + 2]
        ) );
        
        return ( frame );
    }
    
    /**
     * Creates a rotation key frame
     * 
     * @param time
     *                frame time
     * @param angle
     *                rotation angle in degrees
     * @param axis
     *                the axis of the rotation
     * @return a new key frame
     */
    public static KeyFrame buildQuaternion4fKeyFrame( float time, float angle, Axis axis )
    {
        KeyFrameQuat4f frame = new KeyFrameQuat4f();
        frame.time = (long)( time * 1000f );
        float radians = FastMath.toRad( angle );
        
        Tuple3f euler = new Tuple3f(0f, 0f, 0f);
        
        switch ( axis )
        {
            case X:
                euler.set( radians, 0f, 0f );
                break;
                
            case Y:
                euler.set( 0f, radians, 0f );
                break;
                
            case Z:
                euler.set( 0f, 0f, radians );
                break;
        }
        
        frame.setValue( Rotations.toQuaternion( euler ) );
        
        return ( frame );
    }
    
    /**
     * Searches the next key frame according to the current time.
     * 
     * @param frames
     * @param currentTime
     *                in milliseconds
     * @return selected key frame index
     */
    public static int searchNextFrame( List<? extends KeyFrame> frames, long currentTime )
    {
        int frame = 0;
        
        while ( frame < frames.size() && frames.get(frame).time < currentTime )
        {
            frame++;
        }
        
        return ( frame );
    }
}
