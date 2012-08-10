package com.dynamo.cr.webmanage;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.ObjectOutputStream;
import java.io.Serializable;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class SendMailTaskParams implements Serializable {
    private static final long serialVersionUID = 1L;

    private String subject;
    private String from;
    private String name;
    private String message;
    private List<Recipient> recipients = new ArrayList<Recipient>();
    private String serverUrl;

    public SendMailTaskParams() {
    }

    public SendMailTaskParams(String subject, String from, String name, String message, List<Recipient> recipients, String serverUrl) {
        this.subject = subject;
        this.from = from;
        this.name = name;
        this.message = message;
        this.recipients = recipients;
        this.serverUrl = serverUrl;
    }

    public String getSubject() {
        return subject;
    }

    public String getFrom() {
        return from;
    }

    public String getName() {
        return name;
    }

    public String getMessage() {
        return message;
    }

    public List<Recipient> getRecipients() {
        return Collections.unmodifiableList(recipients);
    }

    public String getServerUrl() {
        return serverUrl;
    }

    public byte[] serialize() throws IOException {
        ByteArrayOutputStream out = new ByteArrayOutputStream(64 * 1024);
        ObjectOutputStream objectOut = new ObjectOutputStream(out);
        objectOut.writeObject(this);
        objectOut.close();
        return out.toByteArray();
    }
}
