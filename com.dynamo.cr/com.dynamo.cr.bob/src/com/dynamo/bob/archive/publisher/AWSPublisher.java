package com.dynamo.bob.archive.publisher;

import java.util.List;

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

    @Override
    public void Publish() throws CompileExceptionError {
        AWSCredentials credentials = null;
        AmazonS3 client = null;
        Bucket bucket = null;
        boolean writePermission = false;
        try {
            String accessKey = this.getPublisherSettings().getAwsAccessKey();
            String secretKey = this.getPublisherSettings().getAwsSecretKey();
            credentials = new BasicAWSCredentials(accessKey, secretKey);
            client = new AmazonS3Client(credentials);

            String selectedBucket = this.getPublisherSettings().getAwsBucket();
            if (selectedBucket != null) {
                List<Bucket> buckets = client.listBuckets();
                for (Bucket current : buckets) {
                    if (selectedBucket.equals(current.getName())) {
                        bucket = current;
                        break;
                    }
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

        for (String hexDigest : this.getEntries().keySet()) {
            String prefix = this.getPublisherSettings().getAwsPrefix();
            String key = (prefix + "/" + hexDigest).replaceAll("//+", "/");
            try {
                client.putObject(bucket.getName(), key, this.getEntries().get(hexDigest));
            } catch (AmazonS3Exception exception) {
                throw amazonException("unable to upload file, " + exception.getErrorMessage() + ": " + key, exception);
            } catch (Exception exception) {
                throw compileException("unable to upload file: " + key, exception);
            }
        }
    }

}
