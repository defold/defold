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

import javax.xml.namespace.QName;
import javax.xml.stream.Location;
import javax.xml.stream.XMLStreamConstants;
import javax.xml.stream.XMLStreamException;
import javax.xml.stream.XMLStreamReader;

import org.jagatoo.logging.JAGTLog;

/**
 * A set of triangles. It's an INDEXED triangle set.
 * There are several input sources and the p list
 * contains the position of the data to take from these
 * input sources to make a triangle.
 *
 * Let's take an example. We have two inputs :
 * VERTEX and NORMAL, with offsets 0 and 1.
 * You'd read the p indices list like that :
 * <code>
 * for(int i = 0; i < p.length; i+=2) {
 *      vertex = vertices.get(p[i+0]);
 *      normal = normals.get(p[i+1]);
 * }
 * </code>
 *
 * @author Amos Wenger (aka BlueSky)
 * @author Joe LaFata (aka qbproger)
 */
public class XMLTriangles
{

    public int count = -1;
    public String name = null;
    public ArrayList< XMLInput > inputs = new ArrayList< XMLInput >();
    public int[] p = null;

    public void parse( XMLStreamReader parser, boolean polyList ) throws XMLStreamException
    {
        doParsing( parser, polyList );

        Location loc = parser.getLocation();
        if ( count == -1 )
            JAGTLog.exception( loc.getLineNumber(), ":", loc.getColumnNumber(), " ", this.getClass().getSimpleName(), ": missing count attribute." );
    }

    private void doParsing( XMLStreamReader parser, boolean polyList ) throws XMLStreamException
    {
        for ( int i = 0; i < parser.getAttributeCount(); i++ )
        {
            QName attr = parser.getAttributeName( i );
            if ( attr.getLocalPart().equals( "count" ) )
            {
                count = Integer.parseInt( parser.getAttributeValue( i ).trim() );
            }
            else if ( attr.getLocalPart().equals( "name" ) )
            {
                name = parser.getAttributeValue( i );
            }
            else
            {
                JAGTLog.exception( "Unsupported ", this.getClass().getSimpleName(), " Attr tag: ", attr.getLocalPart() );
            }
        }

        // DYNAMO: Buffering fix
        boolean parsing_triangles = false;
        boolean parsing_vcount = false;
        StringBuffer triangles_buffer = new StringBuffer();
        StringBuffer vcount_buffer = new StringBuffer();
        for ( int event = parser.next(); event != XMLStreamConstants.END_DOCUMENT; event = parser.next() )
        {
            switch ( event )
            {
                case XMLStreamConstants.START_ELEMENT:
                {
                    if ( parser.getLocalName().equals( "input" ) )
                    {
                        XMLInput input = new XMLInput();
                        input.parse( parser );
                        inputs.add( input );
                        parsing_triangles = false;
                        parsing_vcount = false;
                    }
                    else if ( parser.getLocalName().equals( "p" ) )
                    {
                        //triangles_buffer.append(StAXHelper.parseText( parser ));
                        parsing_triangles = true;
                    }
                    else if ( parser.getLocalName().equals( "vcount" ) )
                    {
                        parsing_vcount = true;
                    }
                    else
                    {
                        parsing_triangles = false;
                        parsing_vcount = false;
                        JAGTLog.exception( "Unsupported ", this.getClass().getSimpleName(), " Start tag: ", parser.getLocalName() );
                    }
                    break;
                }
                case XMLStreamConstants.CHARACTERS:
                {
                    if (parsing_triangles)
                        triangles_buffer.append(parser.getText());
                    else if (parsing_vcount)
                        vcount_buffer.append(parser.getText());
                    break;
                }

                case XMLStreamConstants.END_ELEMENT:
                {
                    if ( parser.getLocalName().equals( "triangles" ) )
                    {
                        p = XMLIntArray.toArray(triangles_buffer.toString());
                        return;
                    }
                    else if ( parser.getLocalName().equals( "polylist" ) )
                    {
                        p = XMLIntArray.toArray(triangles_buffer.toString());
                        int[] vcount = XMLIntArray.toArray(vcount_buffer.toString());
                        int totalVertexCount = 0;
                        for (int vc : vcount) {
                            totalVertexCount += vc;
                        }
                        int elementsPerVertex = p.length / totalVertexCount;

                        List<Integer> trianglesP = new ArrayList<Integer>(1024);

                        int base = 0;
                        for (int vc : vcount) {
                            for (int j = 0; j < vc - 2; ++j) {
                                for (int i = 0; i < elementsPerVertex; ++i) {
                                    trianglesP.add(p[base + 0 * elementsPerVertex + i]);
                                }
                                for (int i = 0; i < elementsPerVertex; ++i) {
                                    trianglesP.add(p[base + (j + 1) * elementsPerVertex + i]);
                                }
                                for (int i = 0; i < elementsPerVertex; ++i) {
                                    trianglesP.add(p[base + (j + 2) * elementsPerVertex + i]);
                                }
                            }
                            base += vc * elementsPerVertex;
                        }

                        int[] pPrim = new int[trianglesP.size()];
                        for (int i = 0; i < pPrim.length; i++) {
                            pPrim[i] = trianglesP.get(i);
                        }

                        count = pPrim.length / (3 * elementsPerVertex);

                        p = pPrim;
                        return;
                    }
                    break;
                }
            }
        }
    }
}
