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

import org.jagatoo.util.timing.TimerInterface;

/**
 * ProfileTimer is used as an internal profiling class for
 * the whole engine.
 * 
 * @author David Yazel
 * @author Marvin Froehlich (aka Qudus)
 * @author Amos Wenger
 */
public class ProfileTimer
{
    public static class ProfileNode
    {
        LogChannel channel;
        int numCalls;
        String name;
        long startTime;
        int recursionCount;
        long totalTime;
        ProfileNode parent;
        public ProfileNode next; // pointer to the next profile node
        ProfileNode children;    // pointer to the children
        
        ProfileNode( LogChannel channel, String name, ProfileNode parent )
        {
            this.channel = channel;
            this.name = name;
            this.parent = parent;
            recursionCount = 0;
            totalTime = 0;
            numCalls = 0;
            next = null;
        }
        
        public LogChannel getChannel()
        {
            return ( channel );
        }
        
        public String getName()
        {
            return ( name );
        }
        
        public ProfileNode getParent()
        {
            return ( parent );
        }
        
        public void printLog( int level )
        {
            // convert total time to nanoseconds
            totalTime = (long) (((double) totalTime * (double) 1000000000) / (double) ProfileTimer.getResolution());
            
            StringBuffer sb = new StringBuffer( 200 );
            
            for ( int i = 0; i < (level * 3); i++ )
                sb.append( ' ' );
            
            sb.append( name );
            sb.append( " calls=" );
            sb.append( numCalls );
            
            if ( (totalTime > 0) && (parent != null) && (parent.totalTime > 0) )
            {
                long percentTime = (1000L * totalTime) / parent.totalTime;
                sb.append( ", percent=" );
                sb.append( (float) percentTime / 10f );
                sb.append( "%, " );
            }
            
            if ( numCalls > 0 )
            {
                sb.append( ", avg time=" );
                sb.append( totalTime / numCalls );
                sb.append( "ns (" );

                sb.append( (totalTime / numCalls) / 1000000f );
                sb.append( "ms)" );
            }
            
            Log.profile( channel, sb.toString() );
            
            ProfileNode child = children;
            
            while ( child != null )
            {
                child.printLog( level + 1 );
                child = child.next;
            }
        }
        
        /**
         * @return the node of the child with the specified name.  It will create
         * a new one if it does not exist and insert it into the child list
         * 
         * @param n
         */
        public ProfileNode getSubNode( String n )
        {
            ProfileNode child = children;
            
            while ( child != null )
            {
                if ( child.name == n )
                {
                    return ( child );
                }
                
                child = child.next;
            }
            
            ProfileNode newChild = new ProfileNode( channel, n, this );
            newChild.next = children;
            children = newChild;
            
            return ( newChild );
        }
        
        /**
         * enters a new profile area.
         */
        void enter()
        {
            numCalls++;
            
            if ( recursionCount == 0 )
            {
                startTime = getTime();
            }
            
            recursionCount++;
        }
        
        /**
         * leaves the current profile area.
         */
        boolean leave()
        {
            if ( (--recursionCount == 0) && (numCalls != 0) )
            {
                long time = getTime();
                totalTime += ( time - startTime );
            }
            
            return ( recursionCount == 0 );
        }
    }
    
    /**
     * Contains a thread's worth of profile nodes.  Since we handle the proper
     * nesting of routines, we need one per thread to manage the stack properly
     * 
     * @author David Yazel
     * @author Marvin Froehlich (aka Qudus)
     * @author Amos Wenger (aka BlueSky)
     */
    public static class ProfileContainer
    {
        ProfileNode root;
        ProfileNode currentNode;
        Thread thread;
        public ProfileContainer next; // linked list of profile containers
        
        public ProfileContainer( LogChannel channel )
        {
            thread = Thread.currentThread();
            next = null;
            currentNode = new ProfileNode( channel, thread.getName(), null );
            root = currentNode;
        }
        
        void startProfile( String name )
        {
            if ( currentNode.getName() != name )
            {
                currentNode = currentNode.getSubNode( name );
            }
            
            currentNode.enter();
        }
        
        void endProfile()
        {
            if ( currentNode.leave() )
            {
                currentNode = currentNode.getParent();
            }
        }
        
        void printLog()
        {
            while ( ( currentNode != root ) && ( root.recursionCount >= 0 ) )
                endProfile();
            
            Log.profile( currentNode.getChannel(), "Log for thread " + thread.getName() );
            root.printLog( 0 );
        }
    }
    
    private static ProfileContainer containers = null;
    private static TimerInterface timer = null;
    private static boolean profilingEnabled = false;
    
    private ProfileTimer()
    {
    }
    
    /**
     * Enabled/disable profiling
     * @param enable
     * @param timerInstance A timer used to profile
     */
    public static void setProfilingEnabled( boolean enable, TimerInterface timerInstance )
    {
        profilingEnabled = enable;
        timer = timerInstance;
    }
    
    public static final boolean isProfilingEnabled()
    {
        return ( profilingEnabled );
    }
    
    private static synchronized ProfileContainer newContainer( LogChannel channel )
    {
        ProfileContainer c = new ProfileContainer( channel );
        c.next = containers;
        containers = c;
        
        return ( c );
    }
    
    /**
     * Start a new profile
     */
    public static void startProfile( LogChannel channel, String name )
    {
        if ( !profilingEnabled )
        {
            return;
        }
        
        ProfileContainer c = containers;
        Thread t = Thread.currentThread();
        
        while ( c != null )
        {
            if ( t == c.thread )
            {
                c.startProfile( name );
                
                return;
            }
            
            c = c.next;
        }
        
        newContainer( channel ).startProfile( name );
    }
    
    /**
     * 
     * @return the name of the current profile
     */
    public static String currentProfileName()
    {
        if ( !profilingEnabled )
        {
            return ( null );
        }
        
        ProfileContainer c = containers;
        Thread t = Thread.currentThread();
        
        while ( c != null )
        {
            if ( t == c.thread )
            {
                return ( c.currentNode.getName() );
            }
            
            c = c.next;
        }
        
        return ( null );
    }
    
    /**
     * End a profile
     *
     */
    public static void endProfile()
    {
        if ( !profilingEnabled )
        {
            return;
        }
        
        ProfileContainer c = containers;
        Thread t = Thread.currentThread();
        
        while ( c != null )
        {
            if ( t == c.thread )
            {
                c.endProfile();
                
                return;
            }
            
            c = c.next;
        }
    }
    
    /**
     * Print all logs.
     * 
     * If timer is null, instantly returns.
     */
    public static void printLogs()
    {
        if ( timer == null )
            return;
        
        ProfileContainer c = containers;
        
        Log.profile( c.currentNode.channel, "Timer resolution = " + ProfileTimer.getResolution() + " ticks/second" );
        Log.profile( c.currentNode.channel, "Timer resolution = " + ((double) ProfileTimer.getResolution() / 1000000000.0) + " ns" );
        
        while ( c != null )
        {
            c.printLog();
            c = c.next;
        }
        
    }
    
    /**
     * @return time in nanoseconds
     */
    public static long getTime()
    {
        return ( timer.getTime() );
    }
    
    /**
     * @return Timer resolution
     */
    public static long getResolution()
    {
        return ( timer.getResolution() );
    }
    
    /*
    public static void main( String[] args )
    {
        ConsoleLog cl = new ConsoleLog( LogLevel.PROFILE );
        Log.getLogManager().registerLog( cl );
        System.out.println( "Timer resolution is " + ProfileTimer.getResolution() + " ns" );
        ProfileTimer.startProfile( "main loop" );
        
        int a = 0;
        for ( int i = 0; i < 10000000; i++ )
            a++;
        
        ProfileTimer.endProfile();
        ProfileTimer.printLogs();
    }
    */
}
