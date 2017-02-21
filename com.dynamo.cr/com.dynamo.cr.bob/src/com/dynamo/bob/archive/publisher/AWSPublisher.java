package com.dynamo.bob.archive.publisher;

import com.amazonaws.auth.AWSCredentialsProvider;
import com.amazonaws.auth.profile.ProfileCredentialsProvider;
import com.amazonaws.services.s3.AmazonS3;
import com.amazonaws.services.s3.AmazonS3Client;
import com.amazonaws.services.s3.model.AccessControlList;
import com.amazonaws.services.s3.model.AmazonS3Exception;
import com.amazonaws.services.s3.model.Grant;
import com.amazonaws.services.s3.model.Permission;
import com.dynamo.bob.CompileExceptionError;

public class AWSPublisher extends Publisher {

    public AWSPublisher(PublisherSettings settings) {
        super(settings);
    }

    private CompileExceptionError amazonException(String reason, Throwable exception) {
        String message = "Amazon S3: " + reason;
        return new CompileExceptionError(message, exception);
    }

    private CompileExceptionError compileException(String reason, Throwable exception) {
        String message = "Unable to upload: " + reason;
        return new CompileExceptionError(message, exception);
    }
    
    private boolean hasWritePermissions(AmazonS3 client, String bucket) {
    	AccessControlList acl = client.getBucketAcl(bucket);
        for (Grant grant : acl.getGrantsAsList()) {
            if (grant.getPermission().equals(Permission.Write) || grant.getPermission().equals(Permission.FullControl)) {
                return true;
            }
        }
        
        return false;
    }

    @Override
    public void Publish() throws CompileExceptionError {
    	if (this.getPublisherSettings().getAmazonBucket() == null) {
    		throw compileException("AWS Bucket is not specified", null);
    	} else if (this.getPublisherSettings().getAmazonPrefix() == null) {
    		throw compileException("AWS Prefix is not specified", null);
    	} else if (this.getPublisherSettings().getAmazonCredentialProfile() == null) {
    		throw compileException("AWS Credential profile is not specified", null);
    	}
    	
        try {
        	String credentialProfile = this.getPublisherSettings().getAmazonCredentialProfile();
    		AWSCredentialsProvider credentials = new ProfileCredentialsProvider(credentialProfile);
    		AmazonS3Client client = new AmazonS3Client(credentials);
        	String bucket = this.getPublisherSettings().getAmazonBucket();
            
            if (client.doesBucketExist(bucket)) {
                if (hasWritePermissions(client, bucket)) {
                	String prefix = this.getPublisherSettings().getAmazonPrefix();
                    for (String hexDigest : this.getEntries().keySet()) {
                        String key = (prefix + "/" + hexDigest).replaceAll("//+", "/");
                        try {
                        	client.putObject(bucket, key, this.getEntries().get(hexDigest));
                        } catch (AmazonS3Exception exception) {
                        	throw amazonException("Unable to upload file, " + exception.getErrorMessage() + ": " + key, exception);
                        }
                    }
                } else {
                	throw amazonException("The account does not have permission to upload resources", null);
                }
            } else {
            	throw amazonException("The bucket specified does not exist: " + bucket, null);
            }
        } catch (AmazonS3Exception exception) {
            throw amazonException(exception.getErrorMessage(), exception);
        } catch (Exception exception) {
        	if (exception instanceof CompileExceptionError) {
        		throw exception;
        	}
        	
            throw compileException("Failed to publish resources to Amazon", exception);
        }
    }

}
