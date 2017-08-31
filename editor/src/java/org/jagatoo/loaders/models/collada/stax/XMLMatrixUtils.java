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

import java.util.StringTokenizer;

/**
 * Utils to read Matrix from a COLLADA file.
 * 
 * @author Amos Wenger (aka BlueSky)
 * @author Joe LaFata (aka qbproger)
 */
public class XMLMatrixUtils {
    
    /**
     * Read a Blender-COLLADA row-major matrix and
     * returns a column-major Vecmath matrix.
     * 
     * This is no longer used.  As far as I can 
     * tell by the collada specification all matrices are
     * column major.
     * 
     * @param str
     * @return the 4x4 XML matrix
     */
    public static XMLMatrix4x4 readRowMajor(String str) {
        XMLMatrix4x4 matrix = new XMLMatrix4x4();
        StringTokenizer tknz = new StringTokenizer(str);
        for(int y = 0; y < 4; y++) {
            for(int x = 0; x < 4; x++) {
                matrix.matrix4f.set(x, y, Float.parseFloat(tknz.nextToken()));
            }
        }
        return matrix;
    }
    
    
    /**
     * Read a Blender-COLLADA column-major matrix and
     * returns a column-major Vecmath matrix.
     * @param str
     * @return the 4x4 XML matrix
     */
    public static XMLMatrix4x4 readColumnMajor(String str) {
        XMLMatrix4x4 matrix = new XMLMatrix4x4();
        StringTokenizer tknz = new StringTokenizer(str);
        for(int x = 0; x < 4; x++) {
            for(int y = 0; y < 4; y++) {
                matrix.matrix4f.set(x, y, Float.parseFloat(tknz.nextToken()));
            }
        }
        return matrix;
    }
    
}
