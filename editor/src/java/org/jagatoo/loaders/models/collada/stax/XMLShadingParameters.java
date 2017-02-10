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

import javax.xml.stream.XMLStreamConstants;
import javax.xml.stream.XMLStreamException;
import javax.xml.stream.XMLStreamReader;

import org.jagatoo.logging.JAGTLog;

/**
 * Parameters for a Constan, Lambert, Phong, or Blinn shading.
 * Child of ProfileCOMMON_Technique
 * 
 * @author Amos Wenger (aka BlueSky)
 * @author Joe LaFata (aka qbproger)
 */
public class XMLShadingParameters {
    
    public XMLColorOrTexture emission = null;
    public XMLColorOrTexture ambient = null;
    public XMLColorOrTexture diffuse = null;
    public XMLColorOrTexture specular = null;
    public XMLFloat shininess = null;
    public XMLColorOrTexture reflective = null;
    public XMLFloat reflectivity = null;
    public XMLColorOrTexture transparent = null;
    public XMLFloat transparency = null;
    
    public void parse( XMLStreamReader parser, String endTag ) throws XMLStreamException
    {
        for ( int event = parser.next(); event != XMLStreamConstants.END_DOCUMENT; event = parser.next() )
        {
            switch ( event )
            {
                case XMLStreamConstants.START_ELEMENT:
                {
                    String localName = parser.getLocalName();
                    if ( localName.equals( "emission" ) )
                    {
                        emission = new XMLColorOrTexture();
                        emission.parse( parser, "emission" );
                    }
                    else if ( localName.equals( "ambient" ) )
                    {
                        ambient = new XMLColorOrTexture();
                        ambient.parse( parser, "ambient" );
                    }
                    else if ( localName.equals( "diffuse" ) )
                    {
                        diffuse = new XMLColorOrTexture();
                        diffuse.parse( parser, "diffuse" );
                    }
                    else if ( localName.equals( "specular" ) )
                    {
                        specular = new XMLColorOrTexture();
                        specular.parse( parser, "specular" );
                    }
                    else if ( localName.equals( "shininess" ) )
                    {
                        shininess = new XMLFloat();
                        shininess.parse( parser, "shininess" );
                    }
                    else if ( localName.equals( "reflective" ) )
                    {
                        reflective = new XMLColorOrTexture();
                        reflective.parse( parser, "reflective" );
                    }
                    else if ( localName.equals( "reflectivity" ) )
                    {
                        reflectivity = new XMLFloat();
                        reflectivity.parse( parser, "reflectivity" );
                    }
                    else if ( localName.equals( "transparent" ) )
                    {
                        transparent = new XMLColorOrTexture();
                        transparent.parse( parser, "transparent" );
                    }
                    else if ( localName.equals( "transparency" ) )
                    {
                        transparency = new XMLFloat();
                        transparency.parse( parser, "transparency" );
                    }
                    else
                    {
                        JAGTLog.exception( "Unsupported ", this.getClass().getSimpleName(), " Start tag: ", parser.getLocalName() );
                    }
                    break;
                }
                case XMLStreamConstants.END_ELEMENT:
                {
                    if ( parser.getLocalName().equals( endTag ) )
                        return;
                    break;
                }
            }
        }
    }
}
