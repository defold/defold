package com.dynamo.bob.archive.publisher;

import java.io.File;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.fs.IResource;

public abstract class Publisher {

    private final PublisherSettings settings;
    private final Map<String, File> entries = new HashMap<String, File>();

    public Publisher(PublisherSettings settings) {
        this.settings = settings;
    }

    public String getManifestPublicKey() {
        return this.settings.getManifestPublicKey();
    }
    
    public String getManifestPrivateKey() {
        return this.settings.getManifestPrivateKey();
    }

    protected final PublisherSettings getPublisherSettings() {
        return this.settings;
    }

    protected final Map<String, File> getEntries() {
        return this.entries;
    }

    public abstract void Publish() throws CompileExceptionError;

    public List<IResource> getOutputs(IResource input) {
        List<IResource> outputs = new ArrayList<IResource>();
        return outputs;
    }

    public List<InputStream> getOutputResults() {
        List<InputStream> outputs = new ArrayList<InputStream>();
        return outputs;
    }

    public final void AddEntry(String hexDigest, File fhandle) {
        this.entries.put(hexDigest, fhandle);
    }

}
