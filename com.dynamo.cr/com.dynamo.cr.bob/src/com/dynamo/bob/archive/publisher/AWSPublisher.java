// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

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
import com.dynamo.bob.archive.ArchiveEntry;
import com.dynamo.bob.util.TimeProfiler;

import java.io.File;
import java.io.InputStream;
import java.io.ByteArrayInputStream;
import java.io.SequenceInputStream;

public class AWSPublisher extends Publisher {

    private AmazonS3Client client;

    public AWSPublisher(PublisherSettings settings) {
        super(settings);
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
    public void start() throws CompileExceptionError {
        if (this.getPublisherSettings().getAmazonBucket() == null) {
            throw new CompileExceptionError("AWS Bucket is not specified");
        }
        else if (this.getPublisherSettings().getAmazonPrefix() == null) {
            throw new CompileExceptionError("AWS Prefix is not specified");
        }
        else if (this.getPublisherSettings().getAmazonCredentialProfile() == null) {
            throw new CompileExceptionError("AWS Credential profile is not specified");
        }

        String credentialProfile = this.getPublisherSettings().getAmazonCredentialProfile();
        AWSCredentialsProvider credentials = new ProfileCredentialsProvider(credentialProfile);
        client = new AmazonS3Client(credentials);
        String bucket = this.getPublisherSettings().getAmazonBucket();

        try {
            if (!client.doesBucketExist(bucket)) {
                throw new CompileExceptionError("AWS Bucket '" + bucket + "'does not exist");
            }
            if (!hasWritePermissions(client, bucket)) {
                throw new CompileExceptionError("AWS Account does not have permission to upload resources");
            }
        }
        catch (CompileExceptionError e) {
            throw e;
        } catch (Exception exception) {
            throw new CompileExceptionError("AWS Failed to publish resources", exception);
        }
    }

    @Override
    public void stop() throws CompileExceptionError {

    }

    private void putObject(ArchiveEntry archiveEntry, InputStream data, InputStream header) {
        String bucket = this.getPublisherSettings().getAmazonBucket();
        String prefix = this.getPublisherSettings().getAmazonPrefix();
        String hexDigest = archiveEntry.getHexDigest();
        String key = (prefix + "/" + hexDigest).replaceAll("//+", "/");
        SequenceInputStream seq = new SequenceInputStream(header, data);
        client.putObject(bucket, key, seq, null);
    }

    @Override
    public void publish(ArchiveEntry archiveEntry, InputStream data) throws CompileExceptionError {
        try {
            InputStream header = new ByteArrayInputStream(archiveEntry.getHeader());
            putObject(archiveEntry, data, header);
        }
        catch (Exception exception) {
            throw new CompileExceptionError("AWS Unable to publish archive entry", exception);
        }
    }

    @Override
    public void publish(ArchiveEntry archiveEntry, byte[] data) throws CompileExceptionError {
        try {
            InputStream headerStream = new ByteArrayInputStream(archiveEntry.getHeader());
            InputStream dataStream = new ByteArrayInputStream(data);
            putObject(archiveEntry, dataStream, headerStream);
        }
        catch (Exception exception) {
            throw new CompileExceptionError("AWS Unable to publish archive entry", exception);
        }
    }
}
