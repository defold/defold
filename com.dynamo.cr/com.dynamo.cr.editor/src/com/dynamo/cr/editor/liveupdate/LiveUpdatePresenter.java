package com.dynamo.cr.editor.liveupdate;

import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;

import com.amazonaws.auth.AWSCredentialsProvider;
import com.amazonaws.auth.profile.ProfileCredentialsProvider;
import com.amazonaws.services.s3.AmazonS3Client;
import com.amazonaws.services.s3.model.AmazonS3Exception;
import com.amazonaws.services.s3.model.Bucket;
import com.dynamo.bob.archive.publisher.PublisherSettings;
import com.dynamo.cr.client.IBranchClient;
import com.dynamo.cr.editor.Activator;

public class LiveUpdatePresenter {

    private LiveUpdateDialog dialog = null;
    private PublisherSettings settings;
    
    private List<String> getBuckets(boolean setInfo) {
    	List<String> buckets = new ArrayList<String>();
    	try {
        	String credentialProfile = this.getAmazonCredentialProfile();
    		AWSCredentialsProvider credentials = new ProfileCredentialsProvider(credentialProfile);
    		AmazonS3Client client = new AmazonS3Client(credentials);
        	for (Bucket bucket : client.listBuckets()) {
        		buckets.add(bucket.getName());
        	}
        	
        	if (setInfo) {
        		this.dialog.info("Found " + buckets.size() + " buckets on Amazon S3 for profile '" + this.getAmazonCredentialProfile() + "'");
        	}
    	} catch (AmazonS3Exception exception) {
    		this.dialog.warning("Amazon Error: " + exception.getErrorMessage());
    	} catch (Exception exception) {
    		this.dialog.warning("Error: " + exception.getMessage());
    	}
    	
    	return buckets;
    }

    public void updateBuckets(boolean setInfo) {
    	if (this.getAmazonCredentialProfile() != null) {
    		this.dialog.clearBuckets();
    		
    		for (String bucket : this.getBuckets(setInfo)) {
    			this.dialog.addBucket(bucket);
    		}
    		
    		this.dialog.setBucketSelection(this.getAmazonBucket());
    	}
    }
    
    public void updateMode() {
    	this.dialog.addMode(PublisherSettings.PublishMode.Amazon.toString());
    	this.dialog.addMode(PublisherSettings.PublishMode.Zip.toString());
    	this.dialog.setModeSelection(this.getMode().toString());
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
    }

    public void setMode(PublisherSettings.PublishMode value) {
    	this.settings.setMode(value);
    }

    public PublisherSettings.PublishMode getMode() {
    	if (this.settings.getMode() == null) {
    		this.setMode(PublisherSettings.PublishMode.Amazon);
    	}
    	
    	return this.settings.getMode();
    }
    
    public void setAmazonCredentialProfile(String value) {
    	String previous = this.settings.getAmazonCredentialProfile();
    	this.settings.setAmazonCredentialProfile(value);
    	if (value != null && !value.equals(previous)) {
    		this.updateBuckets(true);
    	}
    }
    
    public String getAmazonCredentialProfile() {
    	return this.settings.getAmazonCredentialProfile();
    }

    public void setAmazonBucket(String value) {
    	this.settings.setAmazonBucket(value);
    }

    public String getAmazonBucket() {
        return this.settings.getAmazonBucket();
    }

    public void setAmazonPrefix(String value) {
    	this.settings.setAmazonPrefix(value);
    }

    public String getAmazonPrefix() {
        return this.settings.getAmazonPrefix();
    }

    public void setZipFilepath(String value) {
    	this.settings.setZipFilepath(value);
    }

    public String getZipFilepath() {
        return this.settings.getZipFilepath();
    }

    public void setManifestPublicKey(String value) {
        this.settings.setManifestPublicKey(value);
    }

    public String getManifestPublicKey() {
        return this.settings.getManifestPublicKey();
    }

    public void setManifestPrivateKey(String value) {
        this.settings.setManifestPrivateKey(value);
    }

    public String getManifestPrivateKey() {
        return this.settings.getManifestPrivateKey();
    }

    public void setSupportedVersions(String value) {
        this.settings.setSupportedVersions(value);
    }

    public String getSupportedVersions() {
        return this.settings.getSupportedVersions();
    }

    public void save() {
        IBranchClient branchClient = Activator.getDefault().getBranchClient();
        String location = branchClient.getNativeLocation();
        File root = new File(location);
        File settings = new File(root, "liveupdate.settings");
        PublisherSettings.save(this.settings, settings);
    }

}
