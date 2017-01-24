package com.dynamo.bob.archive.publisher;

import java.io.File;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import com.amazonaws.auth.AWSCredentials;
import com.amazonaws.auth.BasicAWSCredentials;
import com.amazonaws.services.s3.AmazonS3;
import com.amazonaws.services.s3.AmazonS3Client;
import com.amazonaws.services.s3.model.AccessControlList;
import com.amazonaws.services.s3.model.AmazonS3Exception;
import com.amazonaws.services.s3.model.Bucket;
import com.amazonaws.services.s3.model.Grant;
import com.amazonaws.services.s3.model.Permission;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.archive.ArchiveEntry;
import com.dynamo.bob.fs.IResource;

public class AWSPublisher implements IPublisher {

	private PublisherSettings settings;
	private Map<String, File> entries = new HashMap<String, File>();

	@Override
	public void Initialize(PublisherSettings settings) {
		this.settings = settings;
	}
	
	private CompileExceptionError amazonException(String reason, Throwable exception) {
		String message = "Amazon S3: " + reason;
		return new CompileExceptionError(message, exception);
	}
	
	private CompileExceptionError compileException(String reason, Throwable exception) {
		String message = "Unable to upload: " + reason;
		return new CompileExceptionError(message, exception);
	}

	@Override
	public void Finalize() throws CompileExceptionError {
		AWSCredentials credentials = null;
		AmazonS3 client = null;
		Bucket bucket = null;
		boolean writePermission = false;
		try {
			credentials = new BasicAWSCredentials(this.settings.getAwsAccessKey(), this.settings.getAwsSecretKey());
			client = new AmazonS3Client(credentials);

			List<Bucket> buckets = client.listBuckets();
			for (Bucket current : buckets) {
				String selectedBucket = settings.getAwsBucket();
				if (selectedBucket != null && selectedBucket.equals(current.getName())) {
					bucket = current;
					break;
				}
			}

			if (bucket != null) {
				AccessControlList acl = client.getBucketAcl(bucket.getName());
				for (Grant grant : acl.getGrantsAsList()) {
					if (grant.getPermission().equals(Permission.Write)) {
						writePermission = true;
						break;
					} else if (grant.getPermission().equals(Permission.FullControl)) {
						writePermission = true;
						break;
					}
				}
			}
		} catch (AmazonS3Exception exception) {
			if (exception.getErrorCode().equals("InvalidAccessKeyId")) {
				throw amazonException("The access key is invalid", exception);
			} else if (exception.getErrorCode().equals("SignatureDoesNotMatch")) {
				throw amazonException("The secret key doesn't match the access key", exception);
			} else if (exception.getErrorCode().equals("AccessDenied")) {
				throw amazonException("Access denied, cannot list/write to S3", exception);
			} else {
				throw amazonException("Unexpected error: " + exception.getErrorMessage(), exception);
			}
		} catch (Exception exception) {
			throw compileException("communication with Amazon S3 failed", exception);
		}

		if (bucket == null) {
			throw compileException("unable to find the bucket specified,  has it been removed?", null);
		} else if (writePermission == false) {
			throw compileException("there's no write permission to the bucket specified", null);
		}

		for (String hexDigest : this.entries.keySet()) {
			String key = (this.settings.getAwsPrefix() + "/" + hexDigest).replaceAll("//+", "/");
			try {
				client.putObject(bucket.getName(), key, this.entries.get(hexDigest));
			} catch (AmazonS3Exception exception) {
				throw amazonException("unable to upload file, " + exception.getErrorMessage() + ": " + key, exception);
			} catch (Exception exception) {
				throw compileException("unable to upload file: " + key, exception);
			}
		}
	}

	@Override
	public List<IResource> getOutputs(IResource input) {
		List<IResource> outputs = new ArrayList<IResource>();
		return outputs;
	}

	@Override
	public List<InputStream> getOutputResults() {
		List<InputStream> outputs = new ArrayList<InputStream>();
		return outputs;
	}

	@Override
	public void AddEntry(String hexDigest, File fhandle) {
		this.entries.put(hexDigest, fhandle);
	}

}
