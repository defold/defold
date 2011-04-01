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
import java.util.HashMap;

import javax.xml.stream.Location;
import javax.xml.stream.XMLStreamConstants;
import javax.xml.stream.XMLStreamException;
import javax.xml.stream.XMLStreamReader;

import org.jagatoo.logging.JAGTLog;

/**
 * A Library of Images.
 * Child of COLLADA
 * 
 * @author Amos Wenger (aka BlueSky)
 * @author Joe LaFata (aka qbproger)
 */
public class XMLLibraryImages
{
    
    /**
     * This field is written by JiBX and then parsed by the
     * readImages() method and then the imageMap HashMap
     * is written.
     */
    private ArrayList< XMLImage > imagesList = new ArrayList< XMLImage >();
    
    /**
     * A map of all images, which is filled by the readImages()
     * method just after the images ArrayList has been written.
     * key = ID
     * value = Image
     */
    public HashMap< String, XMLImage > images = null;
    
    /**
     * Called just after images has been read, fill
     * the imageMap.
     */
    public void readImages()
    {
        images = new HashMap< String, XMLImage >();
        for ( XMLImage image: imagesList )
        {
            images.put( image.id, image );
        }
        imagesList = null;
    }
    
    public void parse( XMLStreamReader parser ) throws XMLStreamException
    {
        doParsing( parser );
        
        Location loc = parser.getLocation();
        if ( imagesList.isEmpty() )
            JAGTLog.exception( loc.getLineNumber(), ":", loc.getColumnNumber(), " ", this.getClass().getSimpleName(), ": missing images." );
        
        readImages();
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
                    if ( localName.equals( "image" ) )
                    {
                        XMLImage img = new XMLImage();
                        img.parse( parser );
                        imagesList.add( img );
                    }
                    else
                    {
                        JAGTLog.exception( "Unsupported ", this.getClass().getSimpleName(), " Start tag: ", parser.getLocalName() );
                    }
                    break;
                }
                case XMLStreamConstants.END_ELEMENT:
                {
                    if ( parser.getLocalName().equals( "library_images" ) )
                        return;
                    break;
                }
            }
        }
    }
}
