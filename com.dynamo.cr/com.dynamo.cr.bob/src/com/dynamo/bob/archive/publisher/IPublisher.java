package com.dynamo.bob.archive.publisher;

import java.io.File;
import java.io.InputStream;
import java.util.List;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.archive.ArchiveEntry;
import com.dynamo.bob.fs.IResource;

public interface IPublisher {

	public void Initialize(PublisherSettings settings);
	public void Finalize() throws CompileExceptionError;

	public List<IResource> getOutputs(IResource input);
	public List<InputStream> getOutputResults();

    public void AddEntry(String hexDigest, File fhandle);

}
