package com.dynamo.cr.webmanage;

import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.UnsupportedEncodingException;
import java.net.URLEncoder;
import java.util.List;
import java.util.Properties;
import java.util.logging.Level;
import java.util.logging.Logger;

import javax.mail.Message;
import javax.mail.MessagingException;
import javax.mail.Session;
import javax.mail.Transport;
import javax.mail.internet.AddressException;
import javax.mail.internet.InternetAddress;
import javax.mail.internet.MimeMessage;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

public class SendNewsMailTask extends HttpServlet {

    private static final long serialVersionUID = 1L;
    private static final Logger logger = Logger.getLogger(SendNewsMailTask.class.getName());

    private void sendMail(SendMailTaskParams params, Recipient recipient) {
        Properties props = new Properties();
        Session session = Session.getDefaultInstance(props, null);

        try {
            String messageText = params.getMessage();
            messageText += String.format("\nTo unsubscribe follow this link:\n%s/unsubscribe.html#%s,%s",
                                         "http://www.defold.com",
                                         URLEncoder.encode(recipient.getEmail(), "UTF-8"),
                                         recipient.getKey());

            Message msg = new MimeMessage(session);
            msg.setFrom(new InternetAddress(params.getFrom(), params.getName()));
            msg.addRecipient(Message.RecipientType.TO,
                             new InternetAddress(recipient.getEmail()));
            msg.setSubject(params.getSubject());
            msg.setText(messageText);
            Transport.send(msg);
            logger.fine(String.format("Email sent to %s (%s)", recipient.getEmail(), recipient.getKey()));
        } catch (AddressException e) {
            logger.log(Level.SEVERE, "Failted to send email", e);
        } catch (MessagingException e) {
            logger.log(Level.SEVERE, "Failted to send email", e);
        } catch (UnsupportedEncodingException e) {
            logger.log(Level.SEVERE, "Failted to send email", e);
        }
    }

    @Override
    protected void doPost(HttpServletRequest req, HttpServletResponse resp)
            throws ServletException, IOException {

        ObjectInputStream is = new ObjectInputStream(req.getInputStream());
        SendMailTaskParams params = null;
        try {
             params = (SendMailTaskParams) is.readObject();
        } catch (ClassNotFoundException e) {
            throw new ServletException(e);
        } finally {
            is.close();
        }

        List<Recipient> recipients = params.getRecipients();
        for (Recipient recipient : recipients) {
            sendMail(params, recipient);
        }

        resp.setStatus(200);
        logger.info("Mail batch processed");
    }
}
