package com.dynamo.cr.server.archive;

import com.dynamo.cr.proto.Config.Configuration;
import com.dynamo.cr.server.ConfigurationProvider;
import com.dynamo.cr.server.git.archive.ArchiveCache;
import com.dynamo.cr.server.git.archive.GitArchiveProvider;
import org.apache.commons.io.FileUtils;
import org.apache.commons.io.IOUtils;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.mockito.Mockito;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.zip.ZipOutputStream;

public class ArchiveCacheTest {
    Configuration configuration;

    @Before
    public void setUp() throws Exception {
        configuration = new ConfigurationProvider("test_data/crepo_test.config").get();
    }

    @After
    public void tearDown() {
    }

    @Test
    public void testExistingFileConstructor() throws Exception {
        String archiveKey = "00001ae16697b9a2963a7d2f72c3484b034b9300";

        String tempDirPath = createTempDir(archiveKey, configuration.getArchiveCacheRoot());
        String tmpFilename = createZipArchive(tempDirPath, archiveKey, true);

        ArchiveCache archiveCache = new ArchiveCache(configuration);

        String preloadedFilename = archiveCache.get(archiveKey, "", "");
        Assert.assertEquals(tmpFilename, preloadedFilename);
        FileUtils.deleteQuietly(new File(tmpFilename));
    }

    @Test(expected=com.dynamo.cr.server.ServerException.class)
    public void testExistingNonZipFileConstructor() throws Exception {
        GitArchiveProvider mockProvider = Mockito.mock(GitArchiveProvider.class);

        String archiveKey = "00002ae16697b9a2963a7d2f72c3484b034b9300";

        String tempDirPath = createTempDir(archiveKey, configuration.getArchiveCacheRoot());
        String tmpFilename = createTempArchive(tempDirPath, ".tmp"); // Only zip files should be loaded

        ArchiveCache archiveCache = new ArchiveCache(configuration);
        archiveCache.setArchiveProvider(mockProvider);
        Mockito.doThrow(new IOException("test exception"))
            .when(mockProvider)
            .createLocalArchive(Mockito.anyString(), Mockito.anyString(), Mockito.anyString(), Mockito.anyString());

        try {
            archiveCache.get(archiveKey, "", "");
        } finally {
            FileUtils.deleteQuietly(new File(tmpFilename));
        }

    }

    @Test
    public void testCacheEviction() throws Exception {
        String archiveKey = "00003ae16697b9a2963a7d2f72c3484b034b9300";
        String tempDirPath = createTempDir(archiveKey, configuration.getArchiveCacheRoot());
        String tmpFilename = createZipArchive(tempDirPath, archiveKey, true);

        ArchiveCache archiveCache = new ArchiveCache(configuration);

        archiveCache.invalidateAll();
        File tempFile = new File(tmpFilename);
        Assert.assertFalse(tempFile.exists());
    }

    @Test
    public void testInvalidZipArchive() throws Exception {
        String archiveKey = "00004ae16697b9a2963a7d2f72c3484b034b9300";
        String tempDirPath = createTempDir(archiveKey, configuration.getArchiveCacheRoot());
        String tmpFilename = createZipArchive(tempDirPath, archiveKey, false);

        ArchiveCache archiveCache = new ArchiveCache(configuration);

        archiveCache.invalidateAll();
        File tempFile = new File(tmpFilename);
        // The file was invalid and not loaded into cache, so shouldn't get deleted on invalidate
        Assert.assertTrue(tempFile.exists());
        FileUtils.deleteQuietly(tempFile);
    }

    @Test(expected=com.dynamo.cr.server.ServerException.class)
    public void testFetchDeletedArchive() throws Exception {
        GitArchiveProvider mockArchiveProvider = Mockito.mock(GitArchiveProvider.class);

        String archiveKey = "00005ae16697b9a2963a7d2f72c3484b034b9300";
        String tempDirPath = createTempDir(archiveKey, configuration.getArchiveCacheRoot());
        String tmpFilename = createZipArchive(tempDirPath, archiveKey, true);

        ArchiveCache archiveCache = new ArchiveCache(configuration);
        archiveCache.setArchiveProvider(mockArchiveProvider);

        FileUtils.deleteQuietly(new File(tmpFilename));

        Mockito.doThrow(new IOException("test exception"))
            .when(mockArchiveProvider)
            .createLocalArchive(Mockito.anyString(), Mockito.anyString(), Mockito.anyString(), Mockito.anyString());

        archiveCache.get(archiveKey, "", "");
    }

    @Test(expected=com.dynamo.cr.server.ServerException.class)
    public void testGitException() throws Exception {
        GitArchiveProvider archiveProvider = new GitArchiveProvider();

        String archiveKey = "00006ae16697b9a2963a7d2f72c3484b034b9300";
        ArchiveCache archiveCache = new ArchiveCache(configuration);
        archiveCache.setArchiveProvider(archiveProvider);

        // Should throw a ServerException not git exception
        archiveCache.get(archiveKey, null, null);
    }


    public static String createTempArchive(String tempDirPath, String fileEnding) throws IOException {
        File tempFile = new File(tempDirPath + File.separator + "archive" + System.currentTimeMillis() + fileEnding);
        tempFile.getParentFile().mkdirs();
        tempFile.createNewFile(); // Ignore existing files
        tempFile.deleteOnExit();

        return tempFile.getCanonicalPath();
    }

    public static String createZipArchive(String tempDirPath, String fileKey, boolean isValid) throws IOException {

        String filePath = createTempArchive(tempDirPath, ".zip");
        if(isValid) {
            ZipOutputStream zipOut = null;
            try {
                zipOut = new ZipOutputStream(new FileOutputStream(filePath));
                zipOut.setComment(fileKey);
            } finally {
                IOUtils.closeQuietly(zipOut);
            }
        }
        return filePath;
    }

    public static String createTempDir(String fileKey, String baseDirPath) throws IOException {
        String fileDirUri = baseDirPath + File.separator + fileKey.substring(0,2) + File.separator + fileKey.substring(2);
        File tempDir = new File(fileDirUri);
        tempDir.mkdirs();
        return tempDir.getCanonicalPath();
    }
}
