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
import javax.xml.stream.XMLStreamConstants;
import javax.xml.stream.XMLStreamException;
import javax.xml.stream.XMLStreamReader;

import org.jagatoo.util.errorhandling.IncorrectFormatException;
import org.jagatoo.loaders.models.collada.datastructs.animation.Bone;
import org.jagatoo.loaders.models.collada.datastructs.animation.Skeleton;
import org.jagatoo.loaders.models.collada.datastructs.controllers.Influence;
import org.jagatoo.logging.JAGTLog;
import org.openmali.vecmath2.Matrix4f;

/**
 * A Skin. It defines how skeletal animation should be computed.
 * Child of Controller.
 * 
 * @author Amos Wenger (aka BlueSky)
 * @author Joe LaFata (aka qbproger)
 */
public class XMLSkin
{
    
    /**
     * This source defines a joint/bone for each vertex and each joint
     */
    private static final String SKIN_JOINTS_SOURCE = "skin-joints";
    
    /**
     * This source defines a weight for each vertex and each joint
     */
    private static final String SKIN_WEIGHTS_SOURCE = "skin-weights";
    
    public String source = null;
    
    // Here we instantiate it because if not read, it should be identity
    // (so the COLLADA doc says)
    public XMLMatrix4x4 bindShapeMatrix = null;
    
    public ArrayList< XMLSource > sources = new ArrayList< XMLSource >();
    public ArrayList< XMLInput > jointsInputs = null;
    public XMLVertexWeights vertexWeights = null;
    
    private Influence[][] influences = null;
    
    /**
     * Search the "skin-joints" source.
     * Maybe there is a better way get that
     */
    public XMLSource getJointsSource()
    {
        for ( XMLSource source: sources )
        {
            if ( source.id.endsWith( SKIN_JOINTS_SOURCE ) )
            {
                return source;
            }
        }
        throw new IncorrectFormatException( "Could not find source " + SKIN_JOINTS_SOURCE + " in library_controllers" );
    }
    
    /**
     * Search the "skin-weights" source.
     * Maybe there is a better way get that
     */
    public XMLSource getWeightsSource()
    {
        for ( XMLSource source: sources )
        {
            if ( source.id.endsWith( SKIN_WEIGHTS_SOURCE ) )
            {
                return source;
            }
        }
        throw new IncorrectFormatException( "Could not find source " + SKIN_WEIGHTS_SOURCE + " in library_controllers" );
    }
    
    /**
     * Normalize the influences weights
     */
    private void normalizeInfluences()
    {
        // TODO: not yet implemented!
    }
    
    public void buildInfluences( Skeleton skeleton, int numVertices )
    {
        if ( influences != null )
            return;
        
        influences = new Influence[ numVertices ][];
        
        XMLSource jointsSource = getJointsSource();
        
        // get the "skin-weights", maybe it could be done only one time, when the sources array is filled.
        XMLSource weightsSource = getWeightsSource();
        
        int vIndex = 0;
        for ( int i = 0; i < vertexWeights.vcount.ints.length; i++ )
        {
            final int numBones = vertexWeights.vcount.ints[ i ];
            
            influences[ i ] = new Influence[ numBones ];
            
            for ( int j = 0; j < numBones; j++ )
            {
                final int boneIndex = vertexWeights.v.ints[ vIndex + j * 2 + 0 ];
                final int weightIndex = vertexWeights.v.ints[ vIndex + j * 2 + 1 ];
                
                final float weight = weightsSource.floatArray.floats[ weightIndex ];
                
                if ( boneIndex == -1 )
                {
                    influences[ i ][ j ] = new Influence( bindShapeMatrix.matrix4f, weight );
                }
                else
                {
                    final String boneSourceId = jointsSource.idrefArray.idrefs[ boneIndex ];
                    final Bone bone = skeleton.getBoneBySourceId( boneSourceId );
                    
                    /*
                    Matrix4f m = new Matrix4f( bone.invBindMatrix );
                    m.mul( bone.getAbsoluteTransformation() );
                    */
                    Matrix4f m = new Matrix4f( bone.getAbsoluteTransformation() );
                    
                    influences[ i ][ j ] = new Influence( m, weight );
                }
            }
            
            vIndex += numBones * 2;
        }
        
        normalizeInfluences();
    }
    
    /**
     * Build an array of BoneWeight for easy skinning manipulation
     */
    public Influence[] getInfluencesForVertex( int vertexIndex )
    {
        return ( influences[ vertexIndex ] );
    }
    
    public void parse( XMLStreamReader parser ) throws XMLStreamException
    {
        doParsing( parser );
        
        if ( source == null )
            JAGTLog.exception( this.getClass().getSimpleName(), " missing attribute source." );
        
        if ( jointsInputs == null )
            JAGTLog.exception( this.getClass().getSimpleName(), " missing joint." );
        
        if ( vertexWeights == null )
            JAGTLog.exception( this.getClass().getSimpleName(), " missing vertex weights." );
        
        if ( sources.size() < 3 )
            JAGTLog.exception( this.getClass().getSimpleName(), " not enough sources." );
    }
    
    private void doParsing( XMLStreamReader parser ) throws XMLStreamException
    {
        for ( int i = 0; i < parser.getAttributeCount(); i++ )
        {
            QName attr = parser.getAttributeName( i );
            if ( attr.getLocalPart().equals( "source" ) )
            {
                source = XMLIDREFUtils.parse( parser.getAttributeValue( i ) );
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
                    if ( localName.equals( "source" ) )
                    {
                        XMLSource src = new XMLSource();
                        src.parse( parser );
                        sources.add( src );
                    }
                    else if ( localName.equals( "bind_shape_matrix" ) )
                    {
                        if ( bindShapeMatrix != null )
                            JAGTLog.exception( this.getClass().getSimpleName(), " too many bind_shape_matrix tags." );
                        
                        bindShapeMatrix = XMLMatrixUtils.readColumnMajor( StAXHelper.parseText( parser ) );
                    }
                    else if ( localName.equals( "joints" ) )
                    {
                        jointsInputs = getJointInputs( parser );
                    }
                    else if ( localName.equals( "vertex_weights" ) )
                    {
                        vertexWeights = new XMLVertexWeights();
                        vertexWeights.parse( parser );
                    }
                    else
                    {
                        JAGTLog.exception( "Unsupported ", this.getClass().getSimpleName(), " Start tag: ", parser.getLocalName() );
                    }
                    break;
                }
                case XMLStreamConstants.END_ELEMENT:
                {
                    if ( parser.getLocalName().equals( "skin" ) )
                        return;
                    break;
                }
            }
        }
    }
    
    public ArrayList< XMLInput > getJointInputs( XMLStreamReader parser ) throws XMLStreamException
    {
        ArrayList< XMLInput > inputs = new ArrayList< XMLInput >();
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
                    else
                    {
                        JAGTLog.exception( "Unsupported XMLJoint Start tag: ", parser.getLocalName() );
                    }
                }
                case XMLStreamConstants.END_ELEMENT:
                {
                    if ( parser.getLocalName().equals( "joints" ) )
                        return inputs;
                    break;
                }
            }
        }
        return null;
    }
}
