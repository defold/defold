package com.defold.editor.archive.publisher;

import com.defold.editor.CompileExceptionError;

public class NullPublisher extends Publisher {

    public NullPublisher(PublisherSettings settings) {
        super(settings);
    }

    @Override
    public void Publish() throws CompileExceptionError {

    }

}
