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

import javax.xml.stream.XMLStreamConstants;
import javax.xml.stream.XMLStreamException;
import javax.xml.stream.XMLStreamReader;

import org.jagatoo.logging.JAGTLog;

/**
 * Root element of an XML file.
 * 
 * @author Amos Wenger (aka BlueSky)
 * @author Joe LaFata (aka qbproger)
 */
public class XMLCOLLADA {
    
    public String version = null;
    public XMLAsset asset = null;
    public ArrayList< XMLLibraryAnimations > libraryAnimations = new ArrayList< XMLLibraryAnimations >();
    public ArrayList< XMLLibraryControllers > libraryControllers = new ArrayList< XMLLibraryControllers >();
    public ArrayList< XMLLibraryEffects > libraryEffects = new ArrayList< XMLLibraryEffects >();
    public ArrayList< XMLLibraryImages > libraryImages = new ArrayList< XMLLibraryImages >();
    public ArrayList< XMLLibraryMaterials > libraryMaterials = new ArrayList< XMLLibraryMaterials >();
    public ArrayList< XMLLibraryGeometries > libraryGeometries = new ArrayList< XMLLibraryGeometries >();
    public ArrayList< XMLLibraryVisualScenes > libraryVisualScenes = new ArrayList< XMLLibraryVisualScenes >();
 
    public void parse( XMLStreamReader parser ) throws XMLStreamException
    {
        for (int event = parser.next();  
                event != XMLStreamConstants.END_DOCUMENT; event = parser.next() )
        {
            switch ( event )
            {
                case XMLStreamConstants.START_ELEMENT:
                    if ( parser.getLocalName().equals( "COLLADA" ) )
                    {
                        // don't parse anything from this node
                    }
                    else if ( parser.getLocalName().equals( "asset" ) )
                    {
                        if ( asset != null )
                            JAGTLog.exception( this.getClass().getSimpleName(), ": already has asset tag." );
                        
                        asset = new XMLAsset();
                        asset.parse( parser );
                    }
                    else if(parser.getLocalName().equals( "library_geometries" ))
                    {
                        XMLLibraryGeometries geomLib = new XMLLibraryGeometries();
                        geomLib.parse( parser );
                        libraryGeometries.add( geomLib );
                    }
                    else if ( parser.getLocalName().equals( "library_visual_scenes" ) )
                    {
                        XMLLibraryVisualScenes visSceneLib = new XMLLibraryVisualScenes();
                        visSceneLib.parse( parser );
                        libraryVisualScenes.add( visSceneLib );
                    }
                    else if ( parser.getLocalName().equals( "library_materials" ) )
                    {
                        XMLLibraryMaterials matLib = new XMLLibraryMaterials();
                        matLib.parse( parser );
                        libraryMaterials.add( matLib );
                    }
                    else if ( parser.getLocalName().equals( "library_images" ) )
                    {
                        XMLLibraryImages imageLib = new XMLLibraryImages();
                        imageLib.parse( parser );
                        libraryImages.add( imageLib );
                    }
                    else if ( parser.getLocalName().equals( "library_controllers" ) )
                    {
                        XMLLibraryControllers contrLib = new XMLLibraryControllers();
                        contrLib.parse( parser );
                        libraryControllers.add( contrLib );
                    }
                    else if ( parser.getLocalName().equals( "library_effects" ) )
                    {
                        XMLLibraryEffects effectLib = new XMLLibraryEffects();
                        effectLib.parse( parser );
                        libraryEffects.add( effectLib );
                    }
                    else if ( parser.getLocalName().equals( "library_animations" ) )
                    {
                        XMLLibraryAnimations animationLib = new XMLLibraryAnimations();
                        animationLib.parse( parser );
                        libraryAnimations.add( animationLib );
                    }
                    else
                    {
                        JAGTLog.exception( "Unsupported ", this.getClass().getSimpleName(), " Start tag: ", parser.getLocalName() );
                    }
                    break;
            } // end switch
        }
        
        if ( asset == null )
            JAGTLog.exception( this.getClass().getSimpleName(), ": missing asset." );
    }
}
