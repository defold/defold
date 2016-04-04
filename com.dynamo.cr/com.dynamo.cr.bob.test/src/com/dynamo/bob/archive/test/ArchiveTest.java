package com.dynamo.bob.archive.test;

import static org.junit.Assert.assertEquals;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.file.Files;
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

public class ArchiveTest {

    private String contentRoot;
    private File outputDarc;

    private String createDummyFile(String dir, String filepath, byte[] data) throws IOException {
        File tmp = new File(Paths.get(FilenameUtils.concat(dir, filepath)).toString());
        tmp.getParentFile().mkdirs();
        tmp.deleteOnExit();

        FileOutputStream fos = new FileOutputStream(tmp);
        fos.write(data);
        fos.close();

        return tmp.getAbsolutePath();
    }

    @Before
    public void setUp() throws Exception {
        contentRoot = Files.createTempDirectory(null).toFile().getAbsolutePath();
        outputDarc = Files.createTempFile("tmp.darc", "").toFile();
    }

    @After
    public void tearDown() throws IOException {
        FileUtils.deleteDirectory(new File(contentRoot));
        FileUtils.deleteQuietly(outputDarc);
    }

    @Test
    public void testBuilderAndReader() throws IOException {

        // Create
        ArchiveBuilder ab = new ArchiveBuilder(contentRoot);
        ab.add(createDummyFile(contentRoot, "a.txt", "abc123".getBytes()));
        ab.add(createDummyFile(contentRoot, "b.txt", "apaBEPAc e p a".getBytes()));
        ab.add(createDummyFile(contentRoot, "c.txt", "åäöåäöasd".getBytes()));

        // Write
        RandomAccessFile outFile = new RandomAccessFile(outputDarc, "rw");
        outFile.setLength(0);
        ab.write(outFile);
        outFile.close();

        // Read
        ArchiveReader ar = new ArchiveReader(outputDarc.getAbsolutePath());
        ar.read();
        ar.close();
    }

    @Test
    public void testEntriesOrder() throws IOException {

        // Create
        ArchiveBuilder ab = new ArchiveBuilder(FilenameUtils.separatorsToSystem(contentRoot));
        ab.add(FilenameUtils.separatorsToSystem(createDummyFile(contentRoot, "main/a.txt", "abc123".getBytes())));
        ab.add(FilenameUtils.separatorsToSystem(createDummyFile(contentRoot, "main2/a.txt", "apaBEPAc e p a".getBytes())));
        ab.add(FilenameUtils.separatorsToSystem(createDummyFile(contentRoot, "a.txt", "åäöåäöasd".getBytes())));

        // Write
        RandomAccessFile outFile = new RandomAccessFile(outputDarc, "rw");
        outFile.setLength(0);
        ab.write(outFile);
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
}
