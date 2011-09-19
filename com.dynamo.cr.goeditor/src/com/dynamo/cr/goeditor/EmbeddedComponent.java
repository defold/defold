package com.dynamo.cr.goeditor;

import com.google.protobuf.Message;

public class EmbeddedComponent extends Component {

    private Message message;
    private String type;

    public EmbeddedComponent(Message message, String type) {
        this.message = message;
        this.type = type;
    }

    @Override
    public String getFileExtension() {
        return type;
    }

    public Message getMessage() {
        return message;
    }

}
