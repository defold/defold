package com.dynamo.cr.server.mail;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.ObjectOutputStream;
import java.io.Serializable;
import java.util.Map;
import java.util.Map.Entry;

import com.dynamo.cr.proto.Config.EMailTemplate;

public class EMail implements Serializable {

    private static final long serialVersionUID = 2188033276725142962L;
    private String subject;
    private String from;
    private String to;
    private String message;

    private EMail(String from, String to, String subject, String message) {
        this.from = from;
        this.to = to;
        this.subject = subject;
        this.message = message;
    }

    public static EMail format(EMailTemplate template, String to, Map<String, String> params) {
        String subject = template.getSubject();
        String msg = template.getMessage();
        for (Entry<String, String> e : params.entrySet()) {
            String key = String.format("${%s}", e.getKey());
            msg = msg.replace(key, e.getValue());
            subject = subject.replace(key, e.getValue());
        }
        return new EMail(template.getFrom(), to, subject, msg);
    }

    public String getSubject() {
        return subject;
    }

    public void setSubject(String subject) {
        this.subject = subject;
    }

    public String getFrom() {
        return from;
    }

    public void setFrom(String from) {
        this.from = from;
    }

    public String getTo() {
        return to;
    }

    public void setTo(String to) {
        this.to = to;
    }

    public String getMessage() {
        return message;
    }

    public void setMessage(String message) {
        this.message = message;
    }

    public byte[] serialize() {
        try {
            ByteArrayOutputStream bout = new ByteArrayOutputStream();
            ObjectOutputStream os = new ObjectOutputStream(bout);
            os.writeObject(this);
            os.close();
            return bout.toByteArray();
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

}
