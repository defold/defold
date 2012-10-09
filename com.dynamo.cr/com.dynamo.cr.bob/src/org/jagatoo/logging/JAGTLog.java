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
/**
 * :Id: Log.java,v 1.8 2003/02/24 00:13:53 wurp Exp $
 * 
 * :Log: Log.java,v $
 * Revision 1.8  2003/02/24 00:13:53  wurp
 * Formatted all java code for cvs (strictSunConvention.xml)
 * 
 * Revision 1.7  2002/09/23 00:48:39  dilvish
 * New profiling, spatial trees
 * 
 * Revision 1.6  2002/02/12 02:22:27  dilvish
 * Bunch of bug fixes
 * 
 * Revision 1.5  2002/02/06 02:33:59  dilvish
 * Client version 0.60
 * 
 * Revision 1.4  2001/06/20 04:05:42  wurp
 * added log4j.
 * 
 * Revision 1.3  2001/01/28 07:52:20  wurp
 * Removed <dollar> from Id and Log in log comments.
 * Added several new commands to AdminApp
 * Unfortunately, several other changes that I have lost track of.  Try diffing this
 * version with the previous one.
 * 
 * Revision 1.2  2000/12/16 22:07:33  wurp
 * Added Id and Log to almost all of the files that didn't have it.  It's
 * possible that the script screwed something up.  I did a commit and an update
 * right before I ran the script, so if a file is screwed up you should be able
 * to fix it by just going to the version before this one.
 */
package org.jagatoo.logging;

/**
 * The JAGTLog is a simple shortcut implementation for the {@link Log} class.
 * Please only use this for JAGaToo internal logging. For any other case
 * use the Log class itself or write a similar implementation like this.
 * 
 * @see Log
 * 
 * @author David Yazel
 * @author Marvin Froehlich (aka Qudus)
 */
public final class JAGTLog
{
    public static final LogChannel LOG_CHANNEL = new LogChannel( "JAGaToo" );
    
    /**
     * @return the LogManager to register/deregister {@link LogInterface}s.
     */
    public static final LogManager getLogManager()
    {
        return ( Log.getLogManager() );
    }
    
    /**
     * Sets the String to be prefixed to the actualy logging output n times.
     * 
     * @param indentationString
     */
    public static final void setIndentationString( String indentationString )
    {
        Log.setIndentationString( indentationString );
    }
    
    /**
     * @return the String to be prefixed to the actualy logging output n times.
     */
    public static final String getIndentationString()
    {
        return ( Log.getIndentationString() );
    }
    
    /**
     * Sets the indentation level to use for the following log outputs.
     * 
     * @param indentation
     */
    public static final void setIndentation( int indentation )
    {
        Log.setIndentation( indentation );
    }
    
    /**
     * @return the indentation level to use for the following log outputs.
     */
    public static final int getIndentation()
    {
        return ( Log.getIndentation() );
    }
    
    /**
     * Increases the indentation level to use for the following log outputs by one.
     */
    public static final void increaseIndentation()
    {
        Log.increaseIndentation();
    }
    
    /**
     * Decreases the indentation level to use for the following log outputs by one.
     */
    public static final void decreaseIndentation()
    {
        Log.decreaseIndentation();
    }
    
    /**
     * Prints out a log message without a newline.
     * 
     * @param logLevel
     * @param message
     */
    public static final void print( int logLevel, Object... message )
    {
        Log.print( LOG_CHANNEL, logLevel, message );
    }
    
    /**
     * Prints out a log message with a newline.
     * 
     * @param logLevel
     * @param message
     */
    public static final void println( int logLevel, Object... message )
    {
        Log.println( LOG_CHANNEL, logLevel, message );
    }
    
    /**
     * Dumps an exception to the logging system.
     * 
     * @param exception
     */
    public static final void print( Throwable exception )
    {
        Log.print( LOG_CHANNEL, exception );
    }
    
    /**
     * This is an alias for println( channel, REGULAR, message ).
     * 
     * @param message
     */
    public static final void println( Object... message )
    {
        Log.println( LOG_CHANNEL, LogLevel.REGULAR, message );
    }
    
    /**
     * This is an alias for println( channel, EXHAUSTIVE, message ).
     * 
     * @param message
     */
    public static final void printlnEx( Object... message )
    {
        Log.println( LOG_CHANNEL, LogLevel.EXHAUSTIVE, message );
    }
    
    /**
     * This is an alias for println( channel, ERROR, message ).
     * 
     * @param message
     */
    public static final void error( Object... message )
    {
        Log.println( LOG_CHANNEL, LogLevel.ERROR, message );
    }
    
    /**
     * This is an alias for println( channel, EXCEPTION, message ).
     * 
     * @param message
     */
    public static final void exception( Object... message )
    {
        Log.println( LOG_CHANNEL, LogLevel.EXCEPTION, message );
    }
    
    /**
     * This is an alias for println( channel, DEBUG, message ).
     * 
     * @param message
     */
    public static final void debug( Object... message )
    {
        Log.println( LOG_CHANNEL, LogLevel.DEBUG, message );
    }
    
    /**
     * This is an alias for println( channel, PROFILE, message ).
     * 
     * @param message
     */
    public static final void profile( Object... message )
    {
        Log.println( LOG_CHANNEL, LogLevel.PROFILE, message );
    }
    
    public static final void flush()
    {
        Log.flush();
    }
    
    public static final void close()
    {
        Log.close();
    }
    
    private JAGTLog()
    {
    }
}
