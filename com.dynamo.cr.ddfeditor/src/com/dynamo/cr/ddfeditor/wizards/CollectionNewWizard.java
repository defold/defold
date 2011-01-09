package com.dynamo.cr.ddfeditor.wizards;

import java.io.ByteArrayInputStream;
import java.io.InputStream;

import com.dynamo.gameobject.proto.GameObject.CollectionDesc;
import com.google.protobuf.Message;


public class CollectionNewWizard extends AbstractNewDdfWizard {
    @Override
    public String getTitle() {
        return "Collection binding file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new collection file.";
    }

    @Override
    public String getExtension() {
        return "collection";
    }

    public static Message newInitialWizardContent() {
        return CollectionDesc.newBuilder()
            .setName("unnamed")
            .build();
    }

    @Override
    InputStream openContentStream() {
        String contents = newInitialWizardContent().toString();
        return new ByteArrayInputStream(contents.getBytes());
    }
}
