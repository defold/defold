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
package org.jagatoo.util.timing;

import java.util.ArrayList;

/**
 * Represents a Timestamp in the desired units.
 * Also handles conversions.
 * 
 * @author Amos Wenger (aka BlueSky)
 */
public class Time {
    
    /*
     * Here's a list of all useful time units, with
     * their equivalent in milli-seconds.
     */
    public final static double NANOSECOND   = 0.000001;
    public final static double MICROSECOND  = 0.001;
    public final static double MILLISECOND  = 1;
    public final static double SECOND       = 1000 * MILLISECOND;
    public final static double MINUTE       = 60 * SECOND;
    public final static double HOUR         = 60 * MINUTE;
    public final static double DAY          = 24 * HOUR; // Approximation
    public final static double WEEK         = 7 * DAY; // Approximation
    public final static double MONTH        = 30 * DAY; // Approximation
    public final static double YEAR         = 365 * DAY; // Approximation
    
    static ArrayList<Double> units = new ArrayList<Double>();
    
    static {
        units.add(NANOSECOND);
        units.add(MICROSECOND);
        units.add(MILLISECOND);
        units.add(SECOND);
        units.add(MINUTE);
        units.add(HOUR);
        units.add(DAY);
        units.add(WEEK);
        units.add(MONTH);
        units.add(YEAR);
    }
    
    private double unit;
    private double value;
    
    /**
     * Create a new timestamp
     * @param value The value of the timestamp
     * @param unit The unit of the time stamp, e.g. TimeStamp.NANOSECOND,
     * TimeStamp.MICROSECOND, TimeStamp.MILLISECOND, TimeStamp.SECOND
     */
    public Time(double value, double unit) {
        
        this.value = value;
        this.unit = unit;
        
    }
    
    /**
     * @return the value of this timestamp, in the
     * unit specified.
     * @see #getUnit()
     */
    public double getValue() {
        return value;
    }
    
    /**
     * Set the value of this timestamp, in the unit
     * specified.
     * @param value
     */
    public void setValue(double value) {
        this.value = value;
    }
    
    /**
     * @return the number of milliseconds this TimeStamp
     * represents.
     */
    public double getMilliseconds() {
        return value * unit;
    }
    
    /**
     * @return the unit of this timestamp, = the number
     * of milliseconds ONE unit represents.
     */
    public double getUnit() {
        return unit;
    }
    
    /**
     * @return the converted value to the new unit
     */
    public Time convertTo(double newUnit) {
        return new Time(value * newUnit / unit, newUnit);
    }
    
}
