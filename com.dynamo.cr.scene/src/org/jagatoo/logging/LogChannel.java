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
package org.jagatoo.logging;

/**
 * A LogChannel assotiates a channel id with a name.
 * 
 * @author Marvin Froehlich (aka Qudus)
 */
public class LogChannel
{
    /**
     * This is a bitmask setting, that covers all possible LogChannel IDs.
     */
    public static final int MASK_ALL = 0xFFFFFFFF;
    
    private static int next_id = 1;
    
    private final int id;
    private final String name;
    
    /**
     * @return this channel's ID.
     */
    public final int getID()
    {
        return ( id );
    }
    
    /**
     * @return this channel's name.
     */
    public final String getName()
    {
        return ( name );
    }
    
    /**
     * @return a String to be inserted in the logging output
     */
    public final String getLogString()
    {
        return ( "[ch: " + getName() + "]" );
    }
    
    /**
     * {@inheritDoc}
     */
    @Override
    public int hashCode()
    {
        return ( getID() );
    }
    
    /**
     * {@inheritDoc}
     */
    @Override
    public boolean equals( Object o )
    {
        return ( ( o instanceof LogChannel ) && ( ((LogChannel)o).getID() == this.getID() ) );
    }
    
    
    /**
     * {@inheritDoc}
     */
    @Override
    public String toString()
    {
        return ( this.getClass().getSimpleName() + "{ " + this.getID() + ", \"" + getName() + "\" }" );
    }
    
    public LogChannel( String name )
    {
        this.id = next_id;
        this.name = name;
        
        next_id *= 2;
    }
}
