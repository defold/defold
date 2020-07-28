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

import javax.xml.stream.Location;
import javax.xml.stream.XMLStreamConstants;
import javax.xml.stream.XMLStreamException;
import javax.xml.stream.XMLStreamReader;

import org.jagatoo.logging.JAGTLog;

/**
 * A Binding to a Material.
 * Child of InstanceGeometry and InstanceController.
 * 
 * @author Amos Wenger (aka BlueSky)
 * @author Joe LaFata (aka qbproger)
 */
public class XMLBindMaterial
{
    
    /**
     * A TechniqueCommon, as a child of BindMaterial contains
     * one or more of InstanceMaterial
     * Child of BindMaterial.
     * 
     * @author Amos Wenger (aka BlueSky)
     * @author Joe LaFata (aka qbproger)
     */
    public static class TechniqueCommon
    {
        
        public ArrayList< XMLInstanceMaterial > instanceMaterials = new ArrayList< XMLInstanceMaterial >();
        
        public void parse( XMLStreamReader parser ) throws XMLStreamException
        {
            doParsing( parser );
            if ( instanceMaterials.isEmpty() )
                JAGTLog.exception( this.getClass().getSimpleName(), ": missing Instance Material." );
        }
        
        private void doParsing( XMLStreamReader parser ) throws XMLStreamException
        {
            for ( int event = parser.next(); event != XMLStreamConstants.END_DOCUMENT; event = parser.next() )
            {
                switch ( event )
                {
                    case XMLStreamConstants.START_ELEMENT:
                    {
                        String localName = parser.getLocalName();
                        if ( localName.equals( "instance_material" ) )
                        {
                            XMLInstanceMaterial instMat = new XMLInstanceMaterial();
                            instMat.parse( parser );
                            instanceMaterials.add( instMat );
                        }
                        else
                        {
                            JAGTLog.exception( "Unsupported ", this.getClass().getSimpleName(), " Start tag: ", parser.getLocalName() );
                        }
                        break;
                    }
                    case XMLStreamConstants.END_ELEMENT:
                    {
                        if ( parser.getLocalName().equals( "technique_common" ) )
                            return;
                        break;
                    }
                }
            }
        }
        
    }
    
    public XMLBindMaterial.TechniqueCommon techniqueCommon;
    
    public void parse( XMLStreamReader parser ) throws XMLStreamException
    {
        doParsing( parser );
        
        Location loc = parser.getLocation();
        if ( techniqueCommon == null )
            JAGTLog.exception( loc.getLineNumber(), ":", loc.getColumnNumber(), " ", this.getClass().getSimpleName(), ": missing techniqueCommon." );
    }
    
    private void doParsing( XMLStreamReader parser ) throws XMLStreamException
    {
        for ( int event = parser.next(); event != XMLStreamConstants.END_DOCUMENT; event = parser.next() )
        {
            switch ( event )
            {
                case XMLStreamConstants.START_ELEMENT:
                {
                    String localName = parser.getLocalName();
                    if ( localName.equals( "technique_common" ) )
                    {
                        if ( techniqueCommon != null )
                            JAGTLog.exception( this.getClass().getSimpleName(), " too many ", parser.getLocalName(), " tags." );
                        
                        techniqueCommon = new TechniqueCommon();
                        techniqueCommon.parse( parser );
                    }
                    else
                    {
                        JAGTLog.exception( "Unsupported ", this.getClass().getSimpleName(), " Start tag: ", parser.getLocalName() );
                    }
                    break;
                }
                case XMLStreamConstants.END_ELEMENT:
                {
                    if ( parser.getLocalName().equals( "bind_material" ) )
                        return;
                    break;
                }
            }
        }
    }
}
