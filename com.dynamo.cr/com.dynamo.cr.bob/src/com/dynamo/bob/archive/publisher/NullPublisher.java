package com.dynamo.bob.archive.publisher;

import java.io.File;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.archive.ArchiveEntry;
import com.dynamo.bob.fs.IResource;

public class NullPublisher implements IPublisher {

	private PublisherSettings settings;

	@Override
	public void Initialize(PublisherSettings settings) {
		this.settings = settings;
	}

	@Override
	public void Finalize() throws CompileExceptionError {

	}

	@Override
	public List<IResource> getOutputs(IResource input) {
		List<IResource> outputs = new ArrayList<IResource>();
		return outputs;
	}

	@Override
	public List<InputStream> getOutputResults() {
		List<InputStream> results = new ArrayList<InputStream>();
		return results;
	}

	@Override
	public void AddEntry(String hexDigest, File fhandle) {

	}

}
