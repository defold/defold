package com.dynamo.cr.goeditor;

import com.google.protobuf.Message;

public class EmbeddedComponent extends Component {

    private Message message;

    public EmbeddedComponent(Message message) {
        this.message = message;
    }

    public Message getMessage() {
        return message;
    }

}
