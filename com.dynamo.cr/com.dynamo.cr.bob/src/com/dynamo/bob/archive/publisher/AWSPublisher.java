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

import com.amazonaws.AbortedException;
import com.amazonaws.auth.AWSCredentialsProvider;
import com.amazonaws.auth.profile.ProfileCredentialsProvider;
import com.amazonaws.internal.ResettableInputStream;
import com.amazonaws.services.s3.AmazonS3;
import com.amazonaws.services.s3.AmazonS3Client;
import com.amazonaws.services.s3.model.AccessControlList;
import com.amazonaws.services.s3.model.Grant;
import com.amazonaws.services.s3.model.Permission;
import com.amazonaws.services.s3.model.ObjectMetadata;
import com.amazonaws.services.s3.model.PutObjectRequest;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.IProgress;
import com.dynamo.bob.Project;
import com.dynamo.bob.archive.ArchiveEntry;
import com.dynamo.bob.archive.liveupdate.PackageWriter;

import java.io.IOException;
import java.io.InputStream;
import java.io.ByteArrayInputStream;
import java.io.SequenceInputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.LinkedHashMap;
import java.util.Map;

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
        if (client != null) {
            client.shutdown();
            client = null;
        }
    }

    /** Publish finalized package artifacts, independently of resource compilation and bundle placement. */
    public void publishPackages(Project project, IProgress progress) throws IOException, CompileExceptionError {
        progress.message(IProgress.Message.PublishingLiveUpdate.INSTANCE);
        Map<String, Path> files = new LinkedHashMap<>();
        for (PackageWriter.Output output : PackageWriter.validatedOutputs(project)) {
            checkCanceled(progress);
            if (output.directory()) {
                try (var entries = Files.walk(output.path())) {
                    for (Path file : entries.filter(Files::isRegularFile).sorted().toList()) {
                        addArtifactFile(files, output.name() + "/" + output.path().relativize(file).toString().replace('\\', '/'), file);
                    }
                }
            } else {
                addArtifactFile(files, output.name(), output.path());
            }
        }
        if (files.isEmpty()) return;
        checkCanceled(progress);
        try {
            start();
            var split = progress.split(files.size());
            for (Map.Entry<String, Path> file : files.entrySet()) {
                checkCanceled(progress);
                String key = (getPublisherSettings().getAmazonPrefix() + "/" + file.getKey()).replaceAll("//+", "/");
                // Resettable file streams preserve SDK retries without buffering whole packages in memory.
                try (InputStream input = new ResettableInputStream(file.getValue().toFile()) {
                    @Override
                    public int read() throws IOException {
                        checkCanceled(progress);
                        return super.read();
                    }

                    @Override
                    public int read(byte[] bytes, int offset, int length) throws IOException {
                        checkCanceled(progress);
                        return super.read(bytes, offset, length);
                    }
                }) {
                    ObjectMetadata metadata = new ObjectMetadata();
                    metadata.setContentLength(Files.size(file.getValue()));
                    client.putObject(new PutObjectRequest(getPublisherSettings().getAmazonBucket(), key, input, metadata));
                } catch (Exception exception) {
                    checkCanceled(progress);
                    throw new CompileExceptionError("AWS Unable to publish Live Update artifact '" + file.getKey() +
                            "'; retry with the publish command", exception);
                }
                split.worked();
            }
            checkCanceled(progress);
        } finally {
            stop();
        }
    }

    private static void addArtifactFile(Map<String, Path> files, String name, Path file) throws IOException {
        name = name.replaceAll("//+", "/");
        if (files.putIfAbsent(name, file) != null) {
            throw new IOException("Colliding Live Update publication outputs: " + name);
        }
    }

    private static void checkCanceled(IProgress progress) {
        if (progress.isCanceled() || Thread.currentThread().isInterrupted()) {
            throw new AbortedException("Live Update publication cancelled");
        }
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
