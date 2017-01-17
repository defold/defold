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
 * Declares a data repository that provides values
 * according to the semantics of an <input> element
 * that refers to it.
 * Child of Morph, Animation, Mesh, ConvexMesh, Skin, Spline
 * 
 * @author Amos Wenger (aka BlueSky)
 * @author Joe LaFata (aka qbproger)
 */
public class XMLSource
{
    
    public XMLAsset asset = null;
    
    public String id = null;
    public String name = null;
    
    /*
     * From the COLLADA doc : No more than one of
     * the array elements (<bool_array>, <float_array>,
     * <int_array>, <Name_array>, or <IDREF_array>) may occur.
     * They are mutually exclusive.
     */
    public XMLBoolArray boolArray = null;
    public XMLFloatArray floatArray = null;
    public XMLIntArray intArray = null;
    public XMLNameArray nameArray = null;
    public XMLIDREFArray idrefArray = null;
    
    /**
     * TechniqueCommon, as a child of Source, contains an acessor,
     * which contains the information needed to read a Source.
     * (in fact, it's the meaning of the data in the Source)
     * Child of Source.
     * 
     * @author Amos Wenger (aka BlueSky)
     * @author Joe LaFata (aka qbproger)
     */
    public static class TechniqueCommon
    {
        
        public XMLAccessor accessor = null;
        
        public void parse( XMLStreamReader parser ) throws XMLStreamException
        {
            try
            {
                for ( int event = parser.next(); event != XMLStreamConstants.END_DOCUMENT; event = parser.next() )
                {
                    switch ( event )
                    {
                        case XMLStreamConstants.START_ELEMENT:
                        {
                            if ( parser.getLocalName().equals( "accessor" ) )
                            {
                                accessor = new XMLAccessor();
                                accessor.parse( parser );
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
            finally
            {
                if ( accessor == null )
                    JAGTLog.exception( this.getClass().getSimpleName(), ": missing accessor." );
            }
            
        }
        
    }
    
    public XMLSource.TechniqueCommon techniqueCommon = null;
    
    public void parse( XMLStreamReader parser ) throws XMLStreamException
    {
        Location loc = parser.getLocation();
        if ( !doParsing( parser ) )
            JAGTLog.exception( loc.getLineNumber(), ":", loc.getColumnNumber(), " ", this.getClass().getSimpleName(), ": missing any array." );
    }
    
    private boolean doParsing( XMLStreamReader parser ) throws XMLStreamException
    {
        boolean arrayParsed = false;
        
        for ( int i = 0; i < parser.getAttributeCount(); i++ )
        {
            QName attr = parser.getAttributeName( i );
            if ( attr.getLocalPart().equals( "id" ) )
            {
                id = parser.getAttributeValue( i );
            }
            else if ( attr.getLocalPart().equals( "name" ) )
            {
                name = parser.getAttributeValue( i );
            }
            else
            {
                JAGTLog.println( "Unsupported ", this.getClass().getSimpleName(), " Attr tag: ", attr.getLocalPart() );
            }
        }
        
        for ( int event = parser.next(); event != XMLStreamConstants.END_DOCUMENT; event = parser.next() )
        {
            switch ( event )
            {
                case XMLStreamConstants.START_ELEMENT:
                {
                    String localName = parser.getLocalName();
                    
                    if ( localName.equals( "asset" ) )
                    {
                        asset = new XMLAsset();
                        asset.parse( parser );
                    }
                    else if ( localName.equals( "float_array" ) )
                    {
                        if ( arrayParsed )
                        {
                            JAGTLog.exception( this.getClass().getSimpleName(), " array already processed" );
                        }
                        
                        floatArray = new XMLFloatArray();
                        floatArray.parse( parser );
                        arrayParsed = true;
                    }
                    else if ( localName.equals( "int_array" ) )
                    {
                        if ( arrayParsed )
                        {
                            JAGTLog.exception( this.getClass().getSimpleName(), " array already processed" );
                        }
                        
                        intArray = new XMLIntArray();
                        intArray.parse( parser, "int_array" );
                        arrayParsed = true;
                    }
                    else if ( localName.equals( "bool_array" ) )
                    {
                        if ( arrayParsed )
                        {
                            JAGTLog.exception( this.getClass().getSimpleName(), " array already processed" );
                        }
                        
                        boolArray = new XMLBoolArray();
                        boolArray.parse( parser );
                        arrayParsed = true;
                    }
                    else if ( localName.equals( "Name_array" ) )
                    {
                        if ( arrayParsed )
                        {
                            JAGTLog.exception( this.getClass().getSimpleName(), " array already processed" );
                        }
                        
                        nameArray = new XMLNameArray();
                        nameArray.parse( parser );
                        arrayParsed = true;
                    }
                    else if ( localName.equals( "IDREF_array" ) )
                    {
                        if ( arrayParsed )
                        {
                            JAGTLog.exception( this.getClass().getSimpleName(), " array already processed" );
                        }
                        
                        idrefArray = new XMLIDREFArray();
                        idrefArray.parse( parser );
                        arrayParsed = true;
                    }
                    else if ( localName.equals( "technique_common" ) )
                    {
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
                    if ( parser.getLocalName().equals( "source" ) )
                        return arrayParsed;
                    break;
                }
            }
        }
        return arrayParsed;
    }
    
}
