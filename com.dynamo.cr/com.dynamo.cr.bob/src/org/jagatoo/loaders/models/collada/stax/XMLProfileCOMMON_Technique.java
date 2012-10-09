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

import javax.xml.namespace.QName;
import javax.xml.stream.XMLStreamConstants;
import javax.xml.stream.XMLStreamException;
import javax.xml.stream.XMLStreamReader;

import org.jagatoo.logging.JAGTLog;

/**
 * A Technique used to define the Appearance of an object in
 * the ProfileCOMMON.
 * Child of ProfileCOMMON.
 * 
 * @author Amos Wenger (aka BlueSky)
 * @author Joe LaFata (aka qbproger)
 */
public class XMLProfileCOMMON_Technique {
    
    public String id = null;
    public String sid = null;
    
    public XMLShadingParameters constant = null;
    public XMLShadingParameters lambert = null;
    public XMLShadingParameters phong = null;
    public XMLShadingParameters blinn = null;
    
    private void checkVars( String name )
    {
        if ( constant != null || lambert != null || phong != null || blinn != null )
        {
            JAGTLog.exception( this.getClass().getSimpleName(), " too many ", name, " tags." );
        }
    }
    
    public void parse( XMLStreamReader parser ) throws XMLStreamException
    {
        for ( int i = 0; i < parser.getAttributeCount(); i++ )
        {
            QName attr = parser.getAttributeName( i );
            if ( attr.getLocalPart().equals( "sid" ) )
            {
                sid = parser.getAttributeValue( i );
            }
            else if ( attr.getLocalPart().equals( "id" ) )
            {
                id = parser.getAttributeValue( i );
            }
            else
            {
                JAGTLog.exception( "Unsupported ", this.getClass().getSimpleName(), " Attr tag: ", attr.getLocalPart() );
            }
        }
        
        for ( int event = parser.next(); event != XMLStreamConstants.END_DOCUMENT; event = parser.next() )
        {
            switch ( event )
            {
                case XMLStreamConstants.START_ELEMENT:
                {
                    String localName = parser.getLocalName();
                    if ( localName.equals( "constant" ) )
                    {
                        checkVars( "constant" );
                        constant = new XMLShadingParameters();
                        constant.parse( parser, "constant" );
                    }
                    else if ( localName.equals( "lambert" ) )
                    {
                        checkVars( "lambert" );
                        lambert = new XMLShadingParameters();
                        lambert.parse( parser, "lambert" );
                    }
                    else if ( localName.equals( "phong" ) )
                    {
                        checkVars( "phong" );
                        phong = new XMLShadingParameters();
                        phong.parse( parser, "phong" );
                    }
                    else if ( localName.equals( "blinn" ) )
                    {
                        checkVars( "blinn" );
                        blinn = new XMLShadingParameters();
                        blinn.parse( parser, "blinn" );
                    }
                    else
                    {
                        JAGTLog.exception( "Unsupported ", this.getClass().getSimpleName(), " Start tag: ", parser.getLocalName() );
                    }
                    break;
                }
                case XMLStreamConstants.END_ELEMENT:
                {
                    if ( parser.getLocalName().equals( "technique" ) )
                        return;
                    break;
                }
            }
        }
    }
}
