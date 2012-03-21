package com.dynamo.cr.server.mail;

import java.util.Properties;

import javax.inject.Inject;
import javax.mail.Message;
import javax.mail.MessagingException;
import javax.mail.PasswordAuthentication;
import javax.mail.Session;
import javax.mail.Transport;
import javax.mail.internet.InternetAddress;
import javax.mail.internet.MimeMessage;

import com.dynamo.cr.proto.Config.Configuration;

public class SmtpMailer implements IMailer {

    private Session session;

    @Inject
    public SmtpMailer(Configuration configuration) {
        final String smtpHost = configuration.getSmtpHost();
        final String userName = configuration.getSmtpUsername();
        final String password = configuration.getSmtpPassword();

        Properties props = new Properties();
        props.put("mail.smtp.auth", "true");
        props.put("mail.smtp.starttls.enable", "true");
        props.put("mail.smtp.host", smtpHost);
        props.put("mail.smtp.port", "587");
        session = Session.getInstance(props, new javax.mail.Authenticator() {
            @Override
            protected PasswordAuthentication getPasswordAuthentication() {
                return new PasswordAuthentication(userName, password);
            }
        });
    }

    @Override
    public void send(EMail email) throws MessagingException {
        Message message = new MimeMessage(session);
        message.setFrom(new InternetAddress(email.getFrom()));
        message.setRecipients(Message.RecipientType.TO,
            InternetAddress.parse(email.getTo()));
        message.setSubject(email.getSubject());
        message.setText(email.getMessage());

        Transport.send(message);
    }

}
