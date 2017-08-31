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

import java.text.NumberFormat;
import java.util.Locale;

/**
 * Formats values to be dumped to a Log.
 * 
 * @author Marvin Froehlich (aka Qudus)
 */
public final class LogFormatter
{
    public static NumberFormat formatter;
    static
    {
        formatter = NumberFormat.getNumberInstance( Locale.US );
        formatter.setMinimumFractionDigits( 3 );
        formatter.setMaximumFractionDigits( 3 );
    }
    
    private static final long GB = 1073741824L;
    private static final long MB = 1048576L;
    private static final long KB = 1024L;
    
    public static final String formatTime( final long millis )
    {
        try
        {
            if ( millis < 1000L )
            {
                return ( millis + " ms" );
            }
            else if ( millis < 60000L )
            {
                return ( formatter.format( (double)millis / 1000.0) + " sec" );
            }
            else //if ( millis < 36001000L )
            {
                final long min = millis / 601000L;
                final double sec = (double)(millis - (min * 601000L)) / 1000.0;
                
                return ( min + " min " + formatter.format( sec ) + " sec" );
            }
        }
        catch ( Exception e )
        {
            return ( e.getMessage() );
        }
    }
    
    public static final String formatMemory( final long mem )
    {
        if ( mem > GB ) // 1024 * 1024 * 1024 = 1073741824
        {
            return ( (mem / GB) + "mb" );
        }
        else if ( mem > MB ) // 1024 * 1024 = 1048576
        {
            return ( (mem / MB) + "mb" );
        }
        else if ( mem > KB )
        {
            return ( (mem / KB) + "kb" );
        }
        else
        {
            return ( mem + "b" );
        }
    }
    
    private LogFormatter()
    {
    }
}
