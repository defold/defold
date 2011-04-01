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
package org.jagatoo.loaders.models.collada.stax.test;

import java.io.InputStream;

import javax.xml.stream.XMLInputFactory;
import javax.xml.stream.XMLStreamReader;

import org.jagatoo.loaders.models.collada.stax.XMLCOLLADA;
import org.jagatoo.logging.ConsoleLog;
import org.jagatoo.logging.JAGTLog;

public class Cube {
    
    /**
     * @param args
     * @throws JiBXException
     */
    public static void main(String[] args) throws Exception {
        InputStream stream = Thread.currentThread().getContextClassLoader().getResourceAsStream( "org/jagatoo/loaders/models/collada/stax/models/cube.dae" );
        XMLInputFactory xmlif = XMLInputFactory.newInstance();
        XMLStreamReader reader = xmlif.createXMLStreamReader( stream );

        JAGTLog.getLogManager().registerLog( new ConsoleLog() );
        XMLCOLLADA coll = new XMLCOLLADA();
        long t1 = System.nanoTime();
        coll.parse( reader );
        reader.close();
        long t2 = System.nanoTime();
        
        long t3 = System.nanoTime();
        
        System.out.println("Unmarshalling context creation time = "+((t2 - t1) / 1000000)+" ms");
        System.out.println("Unmarshalling time                  = "+((t3 - t2) / 1000000)+" ms");
        
        System.exit(0);
        
    }
}
