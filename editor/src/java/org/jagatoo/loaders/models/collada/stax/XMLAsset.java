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
package org.jagatoo.loaders.models.collada.stax;

import java.util.ArrayList;
import java.util.List;

import javax.xml.stream.Location;
import javax.xml.stream.XMLStreamConstants;
import javax.xml.stream.XMLStreamException;
import javax.xml.stream.XMLStreamReader;

import org.jagatoo.logging.JAGTLog;
import org.openmali.vecmath2.Vector3f;

/**
 * Asset information about a COLLADA file, e.g.
 * the author, the contributor, the creation/modification
 * dates, units, etc..
 * Child of Camera, COLLADA, Light, Material, Technique,
 * Source, Geometry, Image, Animation, AnimationClip,
 * Controller, Extra, Node, VisualScene, Library_*, Effect,
 * ForceField, PhysicsMaterial, PhysicsScene, PhysicsModel
 * 
 * @author Amos Wenger (aka BlueSky)
 * @author Joe LaFata (aka qbproger)
 */
public class XMLAsset
{
    
    public List< XMLContributor > contributors = new ArrayList< XMLContributor >();
    public String created = null;
    public String modified = null;
    public XMLUnit unit = null;
    
    public static enum UpAxis
    {
        X_UP, Y_UP, Z_UP
    }
    
    public UpAxis upAxis = null;
    
    public final Vector3f getUpVector()
    {
        if ( upAxis == null )
        {
            return Vector3f.POSITIVE_Y_AXIS; // COLLADA-default!
        }
        
        switch ( upAxis )
        {
            case X_UP:
                return Vector3f.POSITIVE_X_AXIS;
            case Y_UP:
                return Vector3f.POSITIVE_Y_AXIS;
            case Z_UP:
                return Vector3f.POSITIVE_Z_AXIS;
        }
        
        return null;
    }
    
    public void parse( XMLStreamReader parser ) throws XMLStreamException
    {
        doParsing( parser );
        Location loc = parser.getLocation();
        if ( created == null )
            JAGTLog.exception( loc.getLineNumber(), ":", loc.getColumnNumber(), " ", this.getClass().getSimpleName(), ": missing created." );
        
        if ( modified == null )
            JAGTLog.exception( loc.getLineNumber(), ":", loc.getColumnNumber(), " ", this.getClass().getSimpleName(), ": missing modified." );
    }
    
    private void doParsing( XMLStreamReader parser ) throws XMLStreamException
    {
        for ( int event = parser.next(); event != XMLStreamConstants.END_DOCUMENT; event = parser.next() )
        {
            switch ( event )
            {
                case XMLStreamConstants.START_ELEMENT:
                {
                    if ( parser.getLocalName().equals( "contributor" ) )
                    {
                        XMLContributor contributor = new XMLContributor();
                        contributor.parse( parser );
                        contributors.add( contributor );
                    }
                    else if ( parser.getLocalName().equals( "created" ) )
                    {
                        if ( created != null )
                            JAGTLog.exception( this.getClass().getSimpleName(), ": already has created." );
                        
                        created = StAXHelper.parseText( parser );
                    }
                    else if ( parser.getLocalName().equals( "modified" ) )
                    {
                        if ( modified != null )
                            JAGTLog.exception( this.getClass().getSimpleName(), ": already has modified." );
                        
                        modified = StAXHelper.parseText( parser );
                    }
                    else if ( parser.getLocalName().equals( "unit" ) )
                    {
                        if ( unit != null )
                            JAGTLog.exception( this.getClass().getSimpleName(), ": already has unit." );
                        
                        unit = new XMLUnit();
                        unit.parse( parser );
                    }
                    else if ( parser.getLocalName().equals( "up_axis" ) )
                    {
                        if ( upAxis != null )
                            JAGTLog.exception( this.getClass().getSimpleName(), ": already has upAxis." );
                        
                        String axis = StAXHelper.parseText( parser );
                        upAxis = UpAxis.valueOf( axis );
                    }
                    else
                    {
                        JAGTLog.exception( "Unsupported ", this.getClass().getSimpleName(), " Start tag: ", parser.getLocalName() );
                    }
                    break;
                }
                case XMLStreamConstants.END_ELEMENT:
                {
                    if ( parser.getLocalName().equals( "asset" ) )
                        return;
                    break;
                }
            } // end switch
        }
    }
}
