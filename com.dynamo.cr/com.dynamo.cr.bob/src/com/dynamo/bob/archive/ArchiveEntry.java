package com.dynamo.bob.archive;

import java.io.IOException;
import java.io.File;

import org.apache.commons.io.FilenameUtils;

public class ArchiveEntry implements Comparable<ArchiveEntry> {
    public static final int FLAG_ENCRYPTED = 1 << 0;
    public static final int FLAG_UNCOMPRESSED = 0xFFFFFFFF;

    // Member vars, TODO make these private and add getters/setters
    public int size; 
    public int compressedSize;
    public int resourceOffset;
    public int flags;
    public String relName;
    public String fileName;
    public byte[] hash = null;

    public ArchiveEntry(String fileName) throws IOException {
        this.fileName = fileName;
    }

    public ArchiveEntry(String root, String fileName, boolean compress) throws IOException {
        File file = new File(fileName);
        if (!file.exists()) {
            throw new IOException(String.format("File %s doens't exists",
                    fileName));
        }

        fileName = file.getAbsolutePath();
        if (!fileName.startsWith(root)) {
            throw new IOException(String.format(
                    "File %s isn't relative to root directory %s",
                    fileName, root));
        }

        this.size = (int) file.length();
        if(compress) {
            // Will be set to real value after compression
            this.compressedSize = 0;
        } else {
            this.compressedSize = FLAG_UNCOMPRESSED;
        }

        this.relName = FilenameUtils.separatorsToUnix(fileName.substring(root.length()));
        this.fileName = fileName;
    }
    
    private int bitwiseCompare(byte b1, byte b2)
    {
    	// Need to compare bit by bit since the byte value is unsigned, but Java has
    	// no unsigned types. We use masking to filter out sign extension when shifting
    	// (https://en.wikipedia.org/wiki/Sign_extension).
    	int ones = 0xFF;
    	for(int i=7; i>=0; i--)
    	{
    		int mask = ones >> i;
    		byte b1_shift = (byte) ((b1 >> i) & mask);
    		byte b2_shift = (byte) ((b2 >> i) & mask);
    		
    		//System.out.println("b1_shift: " + b1_shift + ", b2_shift: " + b2_shift + ", mask = " + mask);
    		
    		if(b1_shift > b2_shift)
				return 1;
			else if(b1_shift < b2_shift)
				return -1;
    	}
    	
    	return 0;
    }

    @Override
    public int compareTo(ArchiveEntry other) {
    	if(this.hash == null)
    		return relName.compareTo(other.relName);
    	else
    	{
    		//System.out.println("-------------");
    		// compare using hash
    		for (int i = 0; i < this.hash.length; i++) {
    			//System.out.println("this: " + this.hash[i]);
    			//System.out.println("other: " + other.hash[i]);
    			int cmp = bitwiseCompare(this.hash[i], other.hash[i]);
    			if(cmp != 0) {
    				return cmp;
    			}
			}
    		return 0;
    	}
    }
}
