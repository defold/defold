package com.dynamo.cr.server.mail;

import java.util.Properties;

import javax.inject.Inject;
import javax.mail.Message;
import javax.mail.MessagingException;
import javax.mail.Session;
import javax.mail.Transport;
import javax.mail.internet.InternetAddress;
import javax.mail.internet.MimeMessage;

import com.dynamo.cr.proto.Config.Configuration;

public class SmtpMailer implements IMailer {

    private String smtpHost;
    private String userName;
    private String password;
    private Session session;

    @Inject
    public SmtpMailer(Configuration configuration) {
        smtpHost = configuration.getSmtpHost();
        userName = configuration.getSmtpUsername();
        password = configuration.getSmtpPassword();

        Properties props = new Properties();
        props.put("mail.smtp.auth", "true");
        props.put("mail.smtp.starttls.enable", "true");
        session = Session.getInstance(props);
    }

    @Override
    public void send(EMail email) throws MessagingException {
        Message message = new MimeMessage(session);
        message.setFrom(new InternetAddress(email.getFrom()));
        message.setRecipients(Message.RecipientType.TO,
            InternetAddress.parse(email.getTo()));
        message.setSubject(email.getSubject());
        message.setText(email.getMessage());

        Transport transport = session.getTransport("smtp");
        transport.connect(smtpHost, 587, userName, password);
        Transport.send(message);
    }

}
