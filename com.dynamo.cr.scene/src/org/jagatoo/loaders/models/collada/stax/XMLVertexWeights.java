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

import javax.xml.namespace.QName;
import javax.xml.stream.Location;
import javax.xml.stream.XMLStreamConstants;
import javax.xml.stream.XMLStreamException;
import javax.xml.stream.XMLStreamReader;

import org.jagatoo.logging.JAGTLog;

/**
 * The Vertex weights information for Skeletal animation.
 * Child of Skin.
 * 
 * @author Amos Wenger (aka BlueSky)
 * @author Joe LaFata (aka qbproger)
 */
public class XMLVertexWeights
{
    
    public int count = -1;
    public ArrayList< XMLInput > inputs = new ArrayList< XMLInput >();
    public XMLIntArray vcount = null;
    public XMLIntArray v = null;
    
    public void parse( XMLStreamReader parser ) throws XMLStreamException
    {
        doParsing( parser );
        
        Location loc = parser.getLocation();
        if ( inputs.size() < 2 )
            JAGTLog.exception( loc.getLineNumber(), ":", loc.getColumnNumber(), " Not enough input tags (2 required)." );
        
        boolean jointFound = false;
        for ( XMLInput in: inputs )
        {
            if ( in.semantic.equals( "JOINT" ) )
            {
                jointFound = true;
                break;
            }
        }
        
        if ( !jointFound )
            JAGTLog.exception( loc.getLineNumber(), ":", loc.getColumnNumber(), " At least 1 input with semantic JOINT needed." );
    }
    
    private void doParsing( XMLStreamReader parser ) throws XMLStreamException
    {
        for ( int i = 0; i < parser.getAttributeCount(); i++ )
        {
            QName attr = parser.getAttributeName( i );
            if ( attr.getLocalPart().equals( "count" ) )
            {
                count = Integer.parseInt( parser.getAttributeValue( i ) );
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
                    if ( localName.equals( "input" ) )
                    {
                        XMLInput input = new XMLInput();
                        input.parse( parser );
                        inputs.add( input );
                    }
                    else if ( localName.equals( "vcount" ) )
                    {
                        if ( vcount != null )
                            JAGTLog.exception( this.getClass().getSimpleName(), " too many ", localName, " tags." );
                        
                        vcount = new XMLIntArray();
                        vcount.parse( parser, "vcount" );
                    }
                    else if ( localName.equals( "v" ) )
                    {
                        if ( v != null )
                            JAGTLog.exception( this.getClass().getSimpleName(), " too many ", localName, " tags." );
                        
                        v = new XMLIntArray();
                        v.parse( parser, "v" );
                    }
                    else
                    {
                        JAGTLog.exception( "Unsupported ", this.getClass().getSimpleName(), " Start tag: ", parser.getLocalName() );
                    }
                    break;
                }
                case XMLStreamConstants.END_ELEMENT:
                {
                    if ( parser.getLocalName().equals( "vertex_weights" ) )
                        return;
                    break;
                }
            }
        }
        
    }
}
