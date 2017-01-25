package com.dynamo.cr.editor.liveupdate;

import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.util.LinkedList;
import java.util.List;

import com.amazonaws.auth.AWSCredentials;
import com.amazonaws.auth.BasicAWSCredentials;
import com.amazonaws.services.s3.AmazonS3;
import com.amazonaws.services.s3.AmazonS3Client;
import com.amazonaws.services.s3.model.AmazonS3Exception;
import com.amazonaws.services.s3.model.Bucket;
import com.dynamo.bob.archive.publisher.PublisherSettings;
import com.dynamo.bob.archive.publisher.PublisherSettings.PublishMode;
import com.dynamo.cr.client.IBranchClient;
import com.dynamo.cr.editor.Activator;

public class LiveUpdatePresenter {

    private LiveUpdateDialog dialog = null;

    private PublisherSettings settings;
    private List<String> buckets = new LinkedList<String>();
    private boolean persistSecretKey = false;

    private boolean hasCredentials() {
    	return getAccessKey() != null && getAccessKey().length() > 0 && getSecretKey() != null && getSecretKey().length() > 0;
    }

    private void fetchBuckets() {
        try {
            this.buckets.clear();
            AWSCredentials credentials = new BasicAWSCredentials(getAccessKey(), getSecretKey());
            AmazonS3 client = new AmazonS3Client(credentials);
            List<Bucket> buckets = client.listBuckets();
            for (Bucket bucket : buckets) {
                this.buckets.add(bucket.getName());
            }
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

    private void populateBuckets() {
        for (String bucket : this.buckets) {
            this.dialog.addBucket(bucket);
        }
    }

    public void updateBuckets() {
    	if (this.hasCredentials()) {
	        this.fetchBuckets();
	        this.populateBuckets();
	        this.dialog.setBucketSelection(getBucket());
    	}
    }

    public void start(LiveUpdateDialog dialog) {
        this.dialog = dialog;
        IBranchClient branchClient = Activator.getDefault().getBranchClient();
        String location = branchClient.getNativeLocation();
        File root = new File(location);
        File settings = new File(root, "liveupdate.settings");
        if (settings.exists()) {
            try {
                InputStream inputStream = new FileInputStream(settings);
                this.settings = PublisherSettings.load(inputStream);
            } catch (Exception exception) {
                this.settings = new PublisherSettings();
            }
        } else {
            this.settings = new PublisherSettings();
        }
        
        if (this.settings.getMode() == null) {
        	this.setMode(PublishMode.Amazon);
        }
    }

    public void setAccessKey(String value) {
        if (value != null) {
            if (!value.equals(settings.getAwsAccessKey())) {
                settings.setValue("liveupdate", "aws-access-key", value);
                this.dialog.info("Amazon S3: Access key specified");
                if (this.hasCredentials()) {
                    this.updateBuckets();
                }
            }
        }
    }

    public String getAccessKey() {
        return settings.getAwsAccessKey();
    }

    public void setSecretKey(String value) {
        if (value != null) {
            if (!value.equals(settings.getAwsSecretKey())) {
                settings.setValue("liveupdate", "aws-secret-key", value);
                this.dialog.info("Amazon S3: Secret key specified");
                if (this.hasCredentials()) {
                    this.updateBuckets();
                }
            }
        }
    }

    public String getSecretKey() {
        return settings.getAwsSecretKey();
    }

    public void setBucket(String value) {
        if (value != null) {
            if (!value.equals(settings.getAwsBucket())) {
                settings.setValue("liveupdate", "aws-bucket", value);
            }
        }
    }

    public String getBucket() {
        return settings.getAwsBucket();
    }

    public void setPrefix(String value) {
        if (value != null) {
            if (!value.equals(settings.getAwsPrefix())) {
                settings.setValue("liveupdate", "aws-prefix", value);
            }
        }
    }

    public String getPrefix() {
        return settings.getAwsPrefix();
    }

    public void setMode(PublisherSettings.PublishMode mode) {
        if (mode != null) {
            settings.setValue("liveupdate", "mode", mode.toString());
        }
    }

    public PublisherSettings.PublishMode getMode() {
        return settings.getMode();
    }

    public void setExportPath(String value) {
        if (value != null) {
            if (!value.equals(settings.getZipFilepath())) {
                settings.setValue("liveupdate", "zip-filepath", value);
            }
        }
    }

    public String getExportPath() {
        return settings.getZipFilepath();
    }

	public void setPersistSecretKey(boolean state) {
		this.persistSecretKey = state;
	}
    
    public void save() {
        IBranchClient branchClient = Activator.getDefault().getBranchClient();
        String location = branchClient.getNativeLocation();
        File root = new File(location);
        File settings = new File(root, "liveupdate.settings");
        
        if (this.persistSecretKey) {
        	PublisherSettings.save(this.settings, settings);
        } else {
        	String awsSecretKey = this.settings.getValue("liveupdate", "aws-secret-key");
        	this.settings.removeValue("liveupdate", "aws-secret-key");
        	PublisherSettings.save(this.settings, settings);
        	this.settings.setValue("liveupdate", "aws-secret-key", awsSecretKey);
        }
        
    }

}
