package com.dynamo.cr.target.bundle;

import java.util.List;

import com.amazonaws.auth.AWSCredentials;
import com.amazonaws.auth.BasicAWSCredentials;
import com.amazonaws.regions.Region;
import com.amazonaws.regions.Regions;
import com.amazonaws.services.s3.AmazonS3;
import com.amazonaws.services.s3.AmazonS3Client;
import com.amazonaws.services.s3.model.AmazonS3Exception;
import com.amazonaws.services.s3.model.Bucket;
import com.amazonaws.services.s3.model.ListObjectsRequest;
import com.amazonaws.services.s3.model.ObjectListing;
import com.amazonaws.services.s3.model.S3ObjectSummary;
import com.dynamo.cr.target.bundle.LiveUpdateDialog.IPresenter;
import com.dynamo.cr.target.bundle.LiveUpdateDialog.Mode;

public class LiveUpdatePresenter implements IPresenter {

	private LiveUpdateDialog dialog = null;
	
	private Mode mode = Mode.UPLOAD_AWS;
	private String privateKeyFilepath = null;
	private String publicKeyFilepath = null;
	
	private String awsAccessKey = null;
	private String awsSecretKey = null;
	private String awsBucket = null;
	private String awsDirectory = null;
	
	private String zipExportFilepath = null;
	
	private boolean hasCredentials() {
		boolean hasAccessKey = this.awsAccessKey != null && this.awsAccessKey.length() > 0;
		boolean hasSecretKey = this.awsSecretKey != null && this.awsSecretKey.length() > 0;
		return hasAccessKey && hasSecretKey;
	}
	
	private void updateBuckets() {
		this.dialog.info("Amazon S3: Fetching buckets ...");
		this.dialog.clearBuckets();
		this.dialog.clearDirectories();
		
		try {
			AWSCredentials credentials = new BasicAWSCredentials(this.awsAccessKey, this.awsSecretKey);
			AmazonS3 client = new AmazonS3Client(credentials);
			List<Bucket> buckets = client.listBuckets();
			for (Bucket bucket : buckets) {
				this.dialog.addBucket(bucket.getName());
			}
			this.dialog.info("Amazon S3: Found " + Integer.toString(buckets.size()) + " buckets");
		} catch (AmazonS3Exception exception) {
			if (exception.getErrorCode().equals("InvalidAccessKeyId")) {
				this.dialog.warning("Amazon S3: The access key is invalid");
			} else if (exception.getErrorCode().equals("SignatureDoesNotMatch")) {
				this.dialog.warning("Amazon S3: The secret key doesn't match the access key");
			} else if (exception.getErrorCode().equals("AccessDenied")) {
				this.dialog.warning("Amazon S3: Access denied, cannot list buckets");
			} else {
				this.dialog.warning("Amazon S3: " + exception.getErrorMessage());
			}
		} catch (Exception ex) {
			this.dialog.warning("Unknown error: " + ex.getMessage());
		}
	}
	
	private void updateDirectories() {
		this.dialog.info("Amazon S3: Fetching directories for " + this.awsBucket + " ...");
		this.dialog.clearDirectories();
		
		try {
			AWSCredentials credentials = new BasicAWSCredentials(this.awsAccessKey, this.awsSecretKey);
			AmazonS3 client = new AmazonS3Client(credentials);
			
			ListObjectsRequest request = new ListObjectsRequest();
			request.withBucketName(this.awsBucket);
			request.withDelimiter("/");
			
			ObjectListing response = client.listObjects(request);
			for (S3ObjectSummary entry : response.getObjectSummaries()) {
				this.dialog.addDirectory(entry.getKey());
			}
			
			this.dialog.info("Amazon S3: Found " + Integer.toString(response.getObjectSummaries().size()) + " directories in " + this.awsBucket);
		} catch (AmazonS3Exception exception) {
			if (exception.getErrorCode().equals("InvalidAccessKeyId")) {
				this.dialog.warning("Amazon S3: The access key is invalid");
			} else if (exception.getErrorCode().equals("SignatureDoesNotMatch")) {
				this.dialog.warning("Amazon S3: The secret key doesn't match the access key");
			} else if (exception.getErrorCode().equals("AccessDenied")) {
				this.dialog.warning("Amazon S3: Access denied, cannot list buckets");
			} else {
				this.dialog.warning("Amazon S3: " + exception.getErrorMessage());
			}
		} catch (Exception ex) {
			this.dialog.warning("Unknown error: " + ex.getMessage());
		}
	}
	
	@Override
	public void start(LiveUpdateDialog dialog) {
		this.dialog = dialog;
	}

	@Override
	public void setPrivateKey(String value) {
		this.privateKeyFilepath = value;
		this.dialog.info("Defold: Private key specified");
	}

	@Override
	public void setPublicKey(String value) {
		this.publicKeyFilepath = value;
		this.dialog.info("Defold: Public key specified");
	}

	@Override
	public void setBucket(String value) {
		if (!value.equals(this.awsBucket)) {
			this.awsBucket = value;
			this.awsDirectory = null;
			this.updateDirectories();
		}
	}

	@Override
	public void setDirectory(String value) {
		if (!value.equals(this.awsDirectory)) {
			this.awsDirectory = value;
			this.dialog.info("Amazon S3: Selected " + this.awsBucket + "/" + this.awsDirectory);
		}
	}

	@Override
	public void setMode(Mode mode) {
		this.mode = mode;
	}

	@Override
	public void setAccessKey(String value) {
		if (!value.equals(this.awsAccessKey)) {
			this.awsAccessKey = value;
			this.dialog.info("Amazon S3: Access key specified");
			if (this.hasCredentials()) {
				this.updateBuckets();
			}
		}
	}

	@Override
	public void setSecretKey(String value) {
		if (!value.equals(this.awsSecretKey)) {
			this.dialog.info("Amazon S3: Secret key specified");
			this.awsSecretKey = value;
			if (this.hasCredentials()) {
				this.updateBuckets();
			}
		}
	}

	@Override
	public void setExportPath(String value) {
		this.zipExportFilepath = value;
	}

}
