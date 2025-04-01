package com.dynamo.bob.archive.test;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.archive.ArchiveEntry;
import com.dynamo.bob.archive.publisher.PublisherSettings;
import com.dynamo.bob.archive.publisher.ZipPublisher;
import org.junit.Test;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ForkJoinPool;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

public class ZipPublisherTest {

    @Test
    public void testCreateArchiveFromFolder() {
        String projectRoot = "test_resources"; // Folder to read files from
        String outputFilename = "test_archive.zip";
        File inputFolder = new File(projectRoot);
        assertTrue("Input folder does not exist " + inputFolder.getAbsolutePath(), inputFolder.exists() && inputFolder.isDirectory());

        PublisherSettings settings = new PublisherSettings();
        settings.setZipFilepath(inputFolder.getAbsoluteFile().getParent()); // Output folder for the zip archive
        settings.setZipFilename(outputFilename);

        ZipPublisher zipPublisher = new ZipPublisher(projectRoot, settings);
        zipPublisher.setFilename(outputFilename);

        try {
            zipPublisher.start();

            // Step 1: Read files and create ArchiveEntry objects
            long startReadTime = System.currentTimeMillis();
            List<Path> toZip = Arrays.asList(Path.of(inputFolder.getAbsolutePath()));

            var archiveMap = new ConcurrentHashMap<ArchiveEntry, byte[]>();
            ForkJoinPool pool = ForkJoinPool.commonPool();
            try {
                toZip.forEach(root -> {
                    try (var pathStream = Files.walk(root)) {
                        pathStream.filter(o -> !Files.isDirectory(o)).forEach(
                                path -> pool.execute(() -> {
                                    BufferedInputStream fileStream = null;
                                    File file = new File(String.valueOf(path));
                                    try {
                                        fileStream = new BufferedInputStream(Files.newInputStream(path));
                                        ArchiveEntry entry = null;
                                        entry = new ArchiveEntry(file.getParentFile().getAbsolutePath(), file.getAbsolutePath());
                                        archiveMap.put(entry, fileStream.readAllBytes());
                                    } catch (IOException e) {
                                        throw new RuntimeException(e);
                                    }
                                }));
                    }
                    catch (IOException e) {
                        throw new RuntimeException(e);
                    }
                });
                pool.awaitQuiescence(Long.MAX_VALUE, java.util.concurrent.TimeUnit.SECONDS);
            } finally {
                pool.shutdown(); // Manually close the pool
            }

            long endReadTime = System.currentTimeMillis();
            long readDuration = endReadTime - startReadTime;
            System.out.println("File reading and entry creation time: " + readDuration + " ms, Entries read: " + archiveMap.size());

            // Step 2: Publish all ArchiveEntry objects
            long startPublishTime = System.currentTimeMillis();
            for (Map.Entry<ArchiveEntry, byte[]> entry : archiveMap.entrySet()) {
                try {
                    zipPublisher.publish(entry.getKey(), entry.getValue());
                } catch (CompileExceptionError e) {
                    fail("Error publishing entry: " + entry.getKey());
                }
            }
            zipPublisher.stop();

            File outputZip = zipPublisher.getZipFile();
            assertTrue("Zip file was not created", outputZip.exists());
            assertTrue("Zip file is empty", outputZip.length() > 0);
            System.out.println("Archive created: " + zipPublisher.getZipFile().getAbsolutePath());

            long endPublishTime = System.currentTimeMillis();
            long publishDuration = endPublishTime - startPublishTime;
            System.out.println("Publishing time: " + publishDuration + " ms, Entries written: " + archiveMap.size());

        } catch (Exception e) {
            e.printStackTrace();
            fail("An error occurred during archiving: " + e.getMessage());
        }
    }
}