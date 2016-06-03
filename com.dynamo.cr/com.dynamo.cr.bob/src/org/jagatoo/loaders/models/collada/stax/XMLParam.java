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
import javax.xml.stream.Location;
import javax.xml.stream.XMLStreamConstants;
import javax.xml.stream.XMLStreamException;
import javax.xml.stream.XMLStreamReader;

import org.jagatoo.logging.JAGTLog;

/**
 * A param is instruction on how to interpret
 * a part of a Source. It has a type and a name.
 * The name contains the "use" of the param, e.g.
 * "TIME", "ANGLE", "X", "Y", "Z"
 * Child of Accessor.
 *
 * @author Amos Wenger (aka BlueSky)
 * @author Joe LaFata (aka qbproger)
 */
public class XMLParam
{

    public static enum Type
    {
        /** Boolean */
        _bool,
        /** Float */
        _float,
        /** Double */
        _double,
        /** Integer */
        _int,
        /** Name (String) */
        _Name,
        /** IDREF (String) */
        _IDREF,
        /** 4x4 float matrix */
        _float4x4
    }

    public XMLParam.Type type = null;

    public static enum Name
    {
        /** X coordinate */
        X,
        /** Y coordinate */
        Y,
        /** Z coordinate */
        Z,
        /** first component for texture coordinates for texture mapping */
        S,
        /** second component for texture coordinates for texture mapping */
        T,
        /** third component for texture coordinates for texture mapping */
        P,
        /** fourth component for texture coordinates for texture mapping */
        Q,
        /** Red component for color */
        R,
        /** Green component for color */
        G,
        /** Blue component for color */
        B,
        /** Alpha component for color */
        A,
        /** Time. Used for anims */
        TIME,
        /** Angle. In degrees. */
        ANGLE,
        /** Weight */
        WEIGHT,
        /** Joint */
        JOINT
    }

    public Name name = null;

    public static Type readTypeString( String typeString )
    {
        return Type.valueOf( "_" + typeString );
    }

    public void parse( XMLStreamReader parser ) throws XMLStreamException
    {
        doParsing( parser );

        Location loc = parser.getLocation();
        if ( name == null )
            JAGTLog.exception( loc.getLineNumber(), ":", loc.getColumnNumber(), " ", this.getClass().getSimpleName(), ": missing name." );

        if ( type == null )
            JAGTLog.exception( loc.getLineNumber(), ":", loc.getColumnNumber(), " ", this.getClass().getSimpleName(), ": missing type." );
    }

    private void doParsing( XMLStreamReader parser ) throws XMLStreamException
    {
        for ( int i = 0; i < parser.getAttributeCount(); i++ )
        {
            QName attr = parser.getAttributeName( i );
            if ( attr.getLocalPart().equals( "name" ) )
            {
                name = Name.valueOf( parser.getAttributeValue( i ).trim() );
            }
            else if ( attr.getLocalPart().equals( "type" ) )
            {
                type = readTypeString( parser.getAttributeValue( i ).trim() );
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
                    JAGTLog.exception( "Unsupported ", this.getClass().getSimpleName(), " Start tag: ", parser.getLocalName() );
                    break;
                }
                case XMLStreamConstants.END_ELEMENT:
                {
                    if ( parser.getLocalName().equals( "param" ) )
                        return;
                    break;
                }
            }
        }
    }

}
