// Copyright 2020-2026 The Defold Foundation
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

package com.dynamo.bob.archive.publisher;

import java.io.IOException;
import java.io.File;
import java.io.InputStream;
import java.io.FileInputStream;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

import com.dynamo.bob.archive.ArchiveEntry;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.fs.IResource;

public abstract class Publisher {

    private final PublisherSettings settings;
    protected final Map<String, ArchiveEntry> entries = new ConcurrentHashMap<>();
    protected String platform = "";

    public Publisher(PublisherSettings settings) {
        this.settings = settings;
    }

    public String getManifestPublicKey() {
        return this.settings.getManifestPublicKey();
    }
    
    public String getManifestPrivateKey() {
        return this.settings.getManifestPrivateKey();
    }

    public IResource getPublisherSettingsResorce() { return this.settings.getResource(); }

    public String getSupportedVersions() {
        return this.settings.getSupportedVersions();
    }

    public boolean shouldBeMovedIntoBundleFolder() { return this.settings.getSaveZipInBundleFolder(); }

    public boolean shouldFolderBeMovedIntoBundleFolder() { return this.settings.getSaveFolderInBundleFolder(); }

    protected final PublisherSettings getPublisherSettings() {
        return this.settings;
    }

    public final Map<String, ArchiveEntry> getEntries() {
        return this.entries;
    }
    
    public String getPlatform() {
        return this.platform;
    }
    
    public void setPlatform(String platform) {
        this.platform = platform;
    }

    /**
     * Call this function before publishing any archive entries. Use
     * this function to make the necessary preparations before accepting
     * entries.
     */
    public abstract void start() throws CompileExceptionError;

    /**
     * Call this function when the publisher should not accept any more
     * entries. Use this function to release any resources needed while
     * publishing.
     */
    public abstract void stop() throws CompileExceptionError;

    /**
     * Publish an entry using this publisher. Make sure to have called
     * start() before calling this function. Also make sure to not call
     * this function after a call to stop().
     * @param archiveEntry The entry to publish
     * @param data The data for the entry
     */
    public abstract void publish(ArchiveEntry archiveEntry, InputStream data) throws CompileExceptionError;

    /**
     * Publish an entry using this publisher. Make sure to have called
     * start() before calling this function. Also make sure to not call
     * this function after a call to stop().
     * @param archiveEntry The entry to publish
     * @param data The data for the entry
     */
    public abstract void publish(ArchiveEntry archiveEntry, byte[] data) throws CompileExceptionError;

    public void publish(ArchiveEntry archiveEntry, File data) throws CompileExceptionError {
        try {
            publish(archiveEntry, new FileInputStream(data));
        }
        catch (IOException e) {
            throw new CompileExceptionError(e);
        }
    }
}
