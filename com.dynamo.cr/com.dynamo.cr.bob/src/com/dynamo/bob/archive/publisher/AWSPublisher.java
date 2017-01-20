package com.dynamo.bob.archive.publisher;

import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

import com.amazonaws.auth.AWSCredentials;
import com.amazonaws.auth.BasicAWSCredentials;
import com.amazonaws.services.s3.AmazonS3;
import com.amazonaws.services.s3.AmazonS3Client;
import com.dynamo.bob.archive.ArchiveEntry;
import com.dynamo.bob.fs.IResource;

public class AWSPublisher implements IPublisher {
	
	private PublisherSettings settings;
	private Map<String, ArchiveEntry> entries;

	@Override
	public void Initialize(PublisherSettings settings) {
		this.settings = settings;
	}
	
	@Override
	public void Finalize() {
		AWSCredentials credentials = null;
		AmazonS3 client = null;
		try {
			credentials = new BasicAWSCredentials(this.settings.awsAccessKey, this.settings.awsSecretKey);
			client = AmazonS3Client(credentials);
			
			// Check if Bucket exists
			// Check if Directory exists
	 		
		}
		
		for (String hexDigest : this.entries.keySet()) {
			ArchiveEntry entry = this.entries.get(hexDigest);
			
		}
	}

	private AmazonS3 AmazonS3Client(AWSCredentials credentials) {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public List<String> getOutputs(IResource input) {
		List<String> outputs = new ArrayList<String>();
		return outputs;
	}

	@Override
	public List<InputStream> getOutputResults() {
		List<InputStream> outputs = new ArrayList<InputStream>();
		return outputs;
	}

	@Override
	public void AddEntry(String hexDigest, ArchiveEntry entry) {
		this.entries.put(hexDigest, entry);
	}

}
