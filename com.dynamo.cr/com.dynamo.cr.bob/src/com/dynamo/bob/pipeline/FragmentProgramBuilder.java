package com.dynamo.bob.pipeline;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.PrintWriter;
import java.io.RandomAccessFile;
import java.util.ArrayList;
import java.util.List;
import java.util.StringTokenizer;

import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CopyBuilder;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;

@BuilderParams(name = "FragmentProgram", inExts = ".fp", outExt = ".fpc")
public class FragmentProgramBuilder extends CopyBuilder {

    @Override
    public void build(Task<Void> task) throws IOException {
        IResource in = task.getInputs().get(0);
        IResource out = task.getOutputs().get(0);
        int INCLUDE_OFFSET = 9;

        String content = new String(in.getContent());
        String absDir = FilenameUtils.separatorsToSystem((FilenameUtils.getPath(in.getAbsPath())));
        
        ArrayList<Integer> includeOffsets = new ArrayList<Integer>();
        ArrayList<Integer> includeSizes = new ArrayList<Integer>();
        ArrayList<String> includePaths = new ArrayList<String>();
        
        String[] lines = content.split("\n");
        
        // Scan input for includes
        int offset = 0;
        //System.out.println("BEGIN!");
        while (offset >= 0 && offset + INCLUDE_OFFSET < content.length()) {
            String contentSubstr = content.substring(offset);
            int i = contentSubstr.indexOf("\n#include");
            
            if (i < 0)
                break;
            
            offset += i;
            
            includeOffsets.add(offset);
            
            offset += INCLUDE_OFFSET;
            //System.out.println("offset: " + offset);
        }
        //System.out.println("DONE!");
        
        int lineNum = 0;
        for (String line : lines) {
            line = line.trim();
            String[] arr = line.split(" ");
            if (arr.length >= 2 && arr[0].equals("#include")) {
                for (int i = 1; i < arr.length; ++i) {
                    if (arr[i].isEmpty() 
                            /*|| (!arr[i].startsWith("\"") || !arr[i].endsWith("\""))
                            || (!arr[i].startsWith("<") || !arr[i].endsWith(">"))*/
                            )
                        continue;
                    System.out.println("arr[i]: " + arr[i] + ", len: " + arr[i].length());
                    arr[i].trim();
                    arr[i] = arr[i].substring(1, arr[i].length()-1);
                    includePaths.add(arr[i].trim());
                    includeSizes.add(line.length() + 1);
                    break;
                }
            }
            ++lineNum;
        }


        ByteArrayOutputStream os = new ByteArrayOutputStream(16 * 1024);
        PrintWriter writer = new PrintWriter(os);
        writer.println("#ifdef GL_ES");
        writer.println("precision mediump float;");
        writer.println("#endif");
        writer.println("#ifndef GL_ES");
        writer.println("#define lowp");
        writer.println("#define mediump");
        writer.println("#define highp");
        writer.println("#endif");
        
        // We want "correct" line numbers from the GLSL compiler.
        //
        // Some Android devices don't like setting #line to something below 1,
        // see JIRA issue: DEF-1786.
        // We still want to have correct line reporting on most devices so
        // only output "#line 0" in debug builds.
        if (project.hasOption("debug")) {
            writer.println("#line 0");
        }

        int i = 0;
        int sourceOffset = 0;
        boolean hasIncludes = false;
        for (String includePath : includePaths) {
            hasIncludes = true;
            
            int includeOffset = includeOffsets.get(i);
            int includeSize = includeSizes.get(i);
            
            String sourceSubstring = content.substring(sourceOffset, includeOffset);
            String includeString = "";
            
            File f = new File("/" + absDir + includePath);
            if (f.exists())
            {
                RandomAccessFile rf = new RandomAccessFile(f, "r");
                rf.seek(0);
                int fileSize = (int)f.length();
                byte[] buf = new byte[fileSize]; // skip EOF
                rf.read(buf);
                rf.close();
                
                includeString = new String(buf);
            }
            
            writer.write(sourceSubstring);
            writer.write(includeString);
            
            sourceOffset = includeOffset + includeSize;
            ++i;
        }
        
        if (sourceOffset < content.length())
        {
            writer.write(content.substring(sourceOffset));
        }
        
        //System.out.println("includePaths size: " + includePaths.size());
        for (String path : includePaths) {
            System.out.println("include rel path: " + path);
        }
        //System.out.println("writer toString: " + writer.toString());
        writer.close();
        //os.write(in.getContent());
        os.close();
        if (hasIncludes) {
            System.out.println("------------------------------------------");
            System.out.println("Complete shader source:");
            System.out.println(new String(os.toByteArray()));
        }
        out.setContent(os.toByteArray());
    }
}
