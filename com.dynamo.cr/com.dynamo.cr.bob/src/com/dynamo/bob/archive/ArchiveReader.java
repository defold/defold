// Copyright 2020-2024 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package com.dynamo.bob.archive;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.RandomAccessFile;
import java.util.ArrayList;
import java.util.List;
import java.util.HashMap;
import java.nio.ByteBuffer;

import com.dynamo.liveupdate.proto.Manifest.ManifestData;
import com.dynamo.liveupdate.proto.Manifest.ManifestFile;
import com.dynamo.liveupdate.proto.Manifest.ResourceEntry;

public class ArchiveReader {
    public static final int VERSION = 5;
    public static final int HASH_BUFFER_BYTESIZE = 64; // 512 bits

    private ArrayList<ArchiveEntry> entries = null;

    private int entryCount = 0;
    private int entryOffset = 0;
    private int hashOffset = 0;
    private int hashLength = 0;

    private final String archiveIndexFilepath;
    private final String archiveDataFilepath;
    private final String manifestFilepath;
    private RandomAccessFile archiveIndexFile = null;
    private RandomAccessFile archiveDataFile = null;
    private ManifestFile manifestFile = null;

    public ArchiveReader(String archiveIndexFilepath, String archiveDataFilepath, String manifestFilepath) {
        this.archiveIndexFilepath = archiveIndexFilepath;
        this.archiveDataFilepath = archiveDataFilepath;
        this.manifestFilepath = manifestFilepath;
    }

    public void read() throws IOException {
        this.archiveIndexFile = new RandomAccessFile(this.archiveIndexFilepath, "r");
        this.archiveDataFile = new RandomAccessFile(this.archiveDataFilepath, "r");

        this.archiveIndexFile.seek(0);
        this.archiveDataFile.seek(0);

        if (this.manifestFilepath != null) {
            InputStream manifestInputStream = new FileInputStream(this.manifestFilepath);
            this.manifestFile = ManifestFile.parseFrom(manifestInputStream);
            manifestInputStream.close();
        }

        // Version
        int indexVersion = this.archiveIndexFile.readInt();
        if (indexVersion == ArchiveReader.VERSION) {
            readArchiveData();
        } else {
            throw new IOException("Unsupported archive index version: " + indexVersion);
        }
    }

    private void readArchiveData() throws IOException {
        // INDEX
        archiveIndexFile.readInt(); // Pad
        archiveIndexFile.readLong(); // UserData, should be 0
        entryCount = archiveIndexFile.readInt();
        entryOffset = archiveIndexFile.readInt();
        hashOffset = archiveIndexFile.readInt();
        hashLength = archiveIndexFile.readInt();

        entries = new ArrayList<ArchiveEntry>(entryCount);

        // Hashes are stored linearly in memory instead of within each entry, so the hashes are read in a separate loop.
        // Once the hashes are read, the rest of the entries are read.

        ManifestData manifestData = null;
        HashMap<ByteBuffer, ResourceEntry> hashToResourceLookup = new HashMap<ByteBuffer, ResourceEntry>();
        if (this.manifestFile != null) {    // some tests do not initialize this.manifestFile
            manifestData = ManifestData.parseFrom(this.manifestFile.getData());
            for (ResourceEntry resource : manifestData.getResourcesList()) {
                hashToResourceLookup.put(ByteBuffer.wrap(resource.getHash().getData().toByteArray(), 0, hashLength), resource);
            }
        }

        // Read entry hashes
        archiveIndexFile.seek(hashOffset);
        for (int i = 0; i < entryCount; ++i) {
            archiveIndexFile.seek(hashOffset + i * HASH_BUFFER_BYTESIZE);
            ArchiveEntry e = new ArchiveEntry("");
            byte[] hash = new byte[HASH_BUFFER_BYTESIZE];
            archiveIndexFile.read(hash, 0, hashLength);
            e.setHash(hash);
            ResourceEntry resource = hashToResourceLookup.get(ByteBuffer.wrap(hash, 0, hashLength));
            if (resource != null) {
                e.setFilename(resource.getUrl());
                e.setRelativeFilename(resource.getUrl());
            }
            entries.add(e);
        }

        // Read entries
        archiveIndexFile.seek(entryOffset);
        for (int i=0; i<entryCount; ++i) {
            ArchiveEntry e = entries.get(i);

            e.setResourceOffset(archiveIndexFile.readInt());
            e.setSize(archiveIndexFile.readInt());
            e.setCompressedSize(archiveIndexFile.readInt());
            e.setFlags(archiveIndexFile.readInt());
        }
    }

    public List<ArchiveEntry> getEntries() {
        return entries;
    }

    public byte[] getEntryContent(ArchiveEntry entry) throws IOException {
        byte[] buf = new byte[entry.getSize()];
        archiveDataFile.seek(entry.getResourceOffset());
        archiveDataFile.read(buf, 0, entry.getSize());

        return buf;
    }

    public void extractAll(String path) throws IOException {

        int entryCount = entries.size();

        System.out.println("Extracting entries to " + path + ": ");
        for (int i = 0; i < entryCount; i++) {
            ArchiveEntry entry = entries.get(i);
            String outdir = path + entry.getFilename();
            System.out.println("> " + entry.getFilename());
            int readSize = entry.getCompressedSize();

            // extract
            byte[] buf = new byte[entry.getSize()];
            archiveDataFile.seek(entry.getResourceOffset());
            archiveDataFile.read(buf, 0, readSize);

            File fo = new File(outdir);
            fo.getParentFile().mkdirs();
            FileOutputStream os = new FileOutputStream(fo);
            os.write(buf);
            os.close();
        }
    }

    public void close() throws IOException {
        if (archiveIndexFile != null) {
            archiveIndexFile.close();
            archiveIndexFile = null;
        }

        if (archiveDataFile != null) {
            archiveDataFile.close();
            archiveDataFile = null;
        }
    }
}
