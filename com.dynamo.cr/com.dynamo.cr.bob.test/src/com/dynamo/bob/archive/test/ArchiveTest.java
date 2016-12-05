package com.dynamo.bob.archive.test;

import static org.junit.Assert.assertEquals;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.List;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.bob.archive.ArchiveEntry;
import com.dynamo.bob.archive.ArchiveBuilder;
import com.dynamo.bob.archive.ArchiveReader;
import com.dynamo.bob.archive.ManifestBuilder;
import com.dynamo.liveupdate.proto.Manifest.HashAlgorithm;

public class ArchiveTest {

    private String contentRoot;
    private File outputDarc;
    private File outputIndex;
    private File outputData;
    private Path resourcePackDir;
    
    private ManifestBuilder manifestBuilder;

    private String createDummyFile(String dir, String filepath, byte[] data) throws IOException {
        File tmp = new File(Paths.get(FilenameUtils.concat(dir, filepath)).toString());
        tmp.getParentFile().mkdirs();
        tmp.deleteOnExit();

        FileOutputStream fos = new FileOutputStream(tmp);
        fos.write(data);
        fos.close();

        return tmp.getAbsolutePath();
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
    		byte b1Shift = (byte) ((b1 >> i) & mask);
    		byte b2Shift = (byte) ((b2 >> i) & mask);
    		if(b1Shift > b2Shift)
				return 1;
			else if(b1Shift < b2Shift)
				return -1;
    	}
    	
    	return 0;
    }

    @Before
    public void setUp() throws Exception {
        contentRoot = Files.createTempDirectory(null).toFile().getAbsolutePath();
        outputDarc = Files.createTempFile("tmp.darc", "").toFile();
        
        outputIndex = Files.createTempFile("tmp.defold", "arci").toFile();
        outputData = Files.createTempFile("tmp.defold", "arcd").toFile();
        
        resourcePackDir = Files.createTempDirectory("tmp.defold.resourcepack_");
        
        manifestBuilder = new ManifestBuilder();
        manifestBuilder.setResourceHashAlgorithm(HashAlgorithm.HASH_SHA1);
    }

    @After
    public void tearDown() throws IOException {
        FileUtils.deleteDirectory(new File(contentRoot));
        FileUtils.deleteQuietly(outputDarc);
        
        FileUtils.deleteQuietly(outputIndex);
        FileUtils.deleteQuietly(outputData);
    }

    @Test
    public void testBuilderAndReader() throws IOException {
        // Create
        ArchiveBuilder ab = new ArchiveBuilder(contentRoot, null);
        ab.add(createDummyFile(contentRoot, "a.txt", "abc123".getBytes()));
        ab.add(createDummyFile(contentRoot, "b.txt", "apaBEPAc e p a".getBytes()));
        ab.add(createDummyFile(contentRoot, "c.txt", "åäöåäöasd".getBytes()));

        // Write
        RandomAccessFile outFile = new RandomAccessFile(outputDarc, "rw");
        outFile.setLength(0);
        ab.write(outFile, null);
        outFile.close();

        // Read
        ArchiveReader ar = new ArchiveReader(outputDarc.getAbsolutePath());
        ar.read();
        ar.close();
    }
    
    @Test
    public void testBuilderAndReader2() throws IOException
    {
    	// Create
    	ArchiveBuilder ab = new ArchiveBuilder(contentRoot, manifestBuilder);
    	ab.add(createDummyFile(contentRoot, "a.txt", "abc123".getBytes()));
    	ab.add(createDummyFile(contentRoot, "b.txt", "apaBEPAc e p a".getBytes()));
        ab.add(createDummyFile(contentRoot, "c.txt", "åäöåäöasd".getBytes()));
        
        // Write
        RandomAccessFile outFileIndex = new RandomAccessFile(outputIndex, "rw");
        RandomAccessFile outFileData = new RandomAccessFile(outputData, "rw");
        outFileIndex.setLength(0);
        outFileData.setLength(0);
        ab.write2(outFileIndex, outFileData, resourcePackDir);
        outFileIndex.close();
        outFileData.close();

        // Read
        ArchiveReader ar = new ArchiveReader(outputIndex.getAbsolutePath(), outputData.getAbsolutePath());
        ar.read2();
        ar.close();
    }

    @Test
    public void testEntriesOrder() throws IOException {

        // Create
        ArchiveBuilder ab = new ArchiveBuilder(FilenameUtils.separatorsToSystem(contentRoot), null);
        ab.add(FilenameUtils.separatorsToSystem(createDummyFile(contentRoot, "main/a.txt", "abc123".getBytes())));
        ab.add(FilenameUtils.separatorsToSystem(createDummyFile(contentRoot, "main2/a.txt", "apaBEPAc e p a".getBytes())));
        ab.add(FilenameUtils.separatorsToSystem(createDummyFile(contentRoot, "a.txt", "åäöåäöasd".getBytes())));

        // Write
        RandomAccessFile outFile = new RandomAccessFile(outputDarc, "rw");
        outFile.setLength(0);
        ab.write(outFile, null);
        outFile.close();

        // Read
        ArchiveReader ar = new ArchiveReader(outputDarc.getAbsolutePath());
        ar.read();
        List<ArchiveEntry> entries = ar.getEntries();
        assertEquals(entries.get(0).fileName, "/a.txt");
        assertEquals(entries.get(1).fileName, "/main/a.txt");
        assertEquals(entries.get(2).fileName, "/main2/a.txt");
        ar.close();
    }
    
    @Test
    public void testEntriesOrder2() throws IOException {

        // Create
        ArchiveBuilder ab = new ArchiveBuilder(FilenameUtils.separatorsToSystem(contentRoot), manifestBuilder);
        ab.add(FilenameUtils.separatorsToSystem(createDummyFile(contentRoot, "main/a.txt", "abc123".getBytes())));
        ab.add(FilenameUtils.separatorsToSystem(createDummyFile(contentRoot, "main2/a.txt", "apaBEPAc e p a".getBytes())));
        ab.add(FilenameUtils.separatorsToSystem(createDummyFile(contentRoot, "a.txt", "åäöåäöasd".getBytes())));

        // Write
        RandomAccessFile outFileIndex = new RandomAccessFile(outputIndex, "rw");
        RandomAccessFile outFileData = new RandomAccessFile(outputData, "rw");
        outFileIndex.setLength(0);
        outFileData.setLength(0);
        ab.write2(outFileIndex, outFileData, resourcePackDir);
        outFileIndex.close();
        outFileData.close();

        // Read
        ArchiveReader ar = new ArchiveReader(outputIndex.getAbsolutePath(), outputData.getAbsolutePath());
        ar.read2();
        List<ArchiveEntry> entries = ar.getEntries();
        
        Boolean correctOrder = false;
        for(int i=1; i<entries.size(); i++) {
            ArchiveEntry ePrev = entries.get(i-1);
            ArchiveEntry eCurr = entries.get(i);
            correctOrder = false;
            for (int j = 0; j < eCurr.hash.length; j++) {
                correctOrder = bitwiseCompare(eCurr.hash[j], ePrev.hash[j]) == 1;
                
                if(correctOrder)
                    break;
            }
            assertEquals(correctOrder, true);
        }
        
        ar.close();
    }
}
