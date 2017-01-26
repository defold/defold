package com.dynamo.bob.archive.publisher;

import com.dynamo.bob.CompileExceptionError;

public class NullPublisher extends Publisher {

    public NullPublisher(PublisherSettings settings) {
        super(settings);
    }

    @Override
    public void Publish() throws CompileExceptionError {

    }

}
