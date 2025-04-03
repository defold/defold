package com.dynamo.bob.archive.test;

import com.dynamo.bob.archive.ArchiveEntry;
import com.dynamo.bob.archive.publisher.PublisherSettings;
import com.dynamo.bob.archive.publisher.ZipPublisher;
import com.dynamo.liveupdate.proto.Manifest;
import org.junit.Test;

import javax.xml.bind.DatatypeConverter;
import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ForkJoinPool;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

public class ZipPublisherTest {
// This isn't a test really, this is setup for development to be able to iterate faster on the archiving
// see https://github.com/defold/defold/issues/10012
//    @Test
    public void testCreateArchiveFromFolder() throws IOException {
        String projectRoot = "test_resources"; // Folder to read files from
        String outputFilename = "test_archive.zip";
        File inputFolder = new File(projectRoot);
        assertTrue("Input folder does not exist " + inputFolder.getAbsolutePath(), inputFolder.exists() && inputFolder.isDirectory());

        PublisherSettings settings = new PublisherSettings();
        settings.setZipFilepath(inputFolder.getAbsoluteFile().getParent()); // Output folder for the zip archive
        settings.setZipFilename(outputFilename);

        FileInputStream fins = new FileInputStream(inputFolder.getAbsolutePath() + "/liveupdate.game.dmanifest");
        Manifest.ManifestFile manifestFile2 = Manifest.ManifestFile.parseFrom(fins);
        fins.close();
        Manifest.ManifestData data = Manifest.ManifestData.parseFrom(manifestFile2.getData());
        HashMap<String, Manifest.ResourceEntry> resources = new HashMap<String, Manifest.ResourceEntry>();
        for (Manifest.ResourceEntry entry : data.getResourcesList()) {
            resources.put(DatatypeConverter.printHexBinary(entry.getHash().getData().toByteArray()).toLowerCase(), entry);
        }

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
                                        ArchiveEntry entry = new ArchiveEntry(file.getParentFile().getAbsolutePath(), file.getAbsolutePath());
                                        Manifest.ResourceEntry _re = resources.get(path.getFileName().toString());
                                        if (_re != null)
                                        {
                                            entry.setCompressedSize(_re.getCompressedSize());
                                            entry.setSize(_re.getSize());
                                            entry.setFlag(_re.getFlags());
                                        }
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
                zipPublisher.publish(entry.getKey(), entry.getValue());
            }
            System.out.println("Write time: " + (System.currentTimeMillis() - startPublishTime));
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