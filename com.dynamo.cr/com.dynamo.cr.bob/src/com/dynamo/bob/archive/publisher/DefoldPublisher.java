package com.dynamo.bob.archive.publisher;

import java.io.File;
import java.io.InputStream;
import java.util.List;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.archive.ArchiveEntry;
import com.dynamo.bob.fs.IResource;

public class DefoldPublisher implements IPublisher {

	private PublisherSettings settings;

	@Override
	public void Initialize(PublisherSettings settings) {
		this.settings = settings;
	}

	@Override
	public void Finalize() throws CompileExceptionError {
		// TODO Auto-generated method stub

	}

	@Override
	public List<IResource> getOutputs(IResource input) {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public List<InputStream> getOutputResults() {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public void AddEntry(String hexDigest, File fhandle) {
		// TODO Auto-generated method stub

	}

}
