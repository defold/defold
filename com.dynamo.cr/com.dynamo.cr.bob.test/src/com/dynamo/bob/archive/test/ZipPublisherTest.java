package com.dynamo.bob.archive.test;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.archive.ArchiveEntry;
import com.dynamo.bob.archive.publisher.PublisherSettings;
import com.dynamo.bob.archive.publisher.ZipPublisher;
import org.junit.Test;
import static org.junit.Assert.*;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.AbstractMap;
import java.util.HashMap;
import java.util.Map;

public class ZipPublisherTest {

    @Test
    public void testCreateArchiveFromFolder() {
        String projectRoot = "test_resources"; // Folder to read files from
        String outputFilename = "test_archive.zip";
        File inputFolder = new File(projectRoot);
        assertTrue("Input folder does not exist " + inputFolder.getAbsolutePath(), inputFolder.exists() && inputFolder.isDirectory());

        PublisherSettings settings = new PublisherSettings();
        settings.setZipFilepath("output"); // Output folder for the zip archive
        settings.setZipFilename(outputFilename);

        ZipPublisher zipPublisher = new ZipPublisher(projectRoot, settings);
        zipPublisher.setFilename(outputFilename);

        try {
            zipPublisher.start();

            // Step 1: Read files and create ArchiveEntry objects
            long startReadTime = System.currentTimeMillis();
            Map<String, AbstractMap.SimpleEntry<ArchiveEntry, byte[]>> archiveMap = new HashMap<>();
            File[] files = inputFolder.listFiles();
            assertNotNull("No files found in the input folder", files);

            for (File file : files) {
                if (file.isFile()) {
                    try (FileInputStream fis = new FileInputStream(file)) {
                        byte[] data = fis.readAllBytes();
                        ArchiveEntry entry = new ArchiveEntry(file.getParentFile().getAbsolutePath(), file.getAbsolutePath());
                        archiveMap.put(entry.getFilename(), new AbstractMap.SimpleEntry<>(entry, data));
                    } catch (IOException e) {
                        fail("Error reading file: " + file.getAbsolutePath());
                    }
                }
            }
            long endReadTime = System.currentTimeMillis();
            long readDuration = endReadTime - startReadTime;
            System.out.println("File reading and entry creation time: " + readDuration + " ms, Entries read: " + archiveMap.size());

            // Step 2: Publish all ArchiveEntry objects
            long startPublishTime = System.currentTimeMillis();
            for (Map.Entry<String, AbstractMap.SimpleEntry<ArchiveEntry, byte[]>> entry : archiveMap.entrySet()) {
                try {
                    zipPublisher.publish(entry.getValue().getKey(), entry.getValue().getValue());
                } catch (CompileExceptionError e) {
                    fail("Error publishing entry: " + entry.getKey());
                }
            }
            long endPublishTime = System.currentTimeMillis();
            long publishDuration = endPublishTime - startPublishTime;
            System.out.println("Publishing time: " + publishDuration + " ms, Entries written: " + archiveMap.size());

            zipPublisher.stop();

            File outputZip = zipPublisher.getZipFile();
            assertTrue("Zip file was not created", outputZip.exists());
            assertTrue("Zip file is empty", outputZip.length() > 0);
            System.out.println("Archive created: " + zipPublisher.getZipFile().getAbsolutePath());

        } catch (Exception e) {
            e.printStackTrace();
            fail("An error occurred during archiving: " + e.getMessage());
        }
    }
}