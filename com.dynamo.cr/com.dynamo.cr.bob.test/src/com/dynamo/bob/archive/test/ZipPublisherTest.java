package com.dynamo.bob.archive.test;

import com.dynamo.bob.archive.ArchiveEntry;
import com.dynamo.bob.archive.publisher.PublisherSettings;
import com.dynamo.bob.archive.publisher.ZipPublisher;
import org.junit.Test;
import static org.junit.Assert.*;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;

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
            long startTime = System.currentTimeMillis();

            File[] files = inputFolder.listFiles();
            assertNotNull("No files found in the input folder", files);

            for (File file : files) {
                if (file.isFile()) {
                    try (FileInputStream fis = new FileInputStream(file)) {
                        ArchiveEntry entry = new ArchiveEntry(file.getParentFile().getAbsolutePath(), file.getAbsolutePath());
                        zipPublisher.publish(entry, fis);
                    } catch (IOException e) {
                        fail("Error reading file: " + file.getAbsolutePath());
                    }
                }
            }

            zipPublisher.stop();
            long endTime = System.currentTimeMillis();
            long duration = endTime - startTime;

            System.out.println("Archive created: " + zipPublisher.getZipFile().getAbsolutePath());
            System.out.println("Time taken: " + duration + " s");

            File outputZip = zipPublisher.getZipFile();
            assertTrue("Zip file was not created", outputZip.exists());
            assertTrue("Zip file is empty", outputZip.length() > 0);

        } catch (Exception e) {
            e.printStackTrace();
            fail("An error occurred during archiving: " + e.getMessage());
        }
    }
}