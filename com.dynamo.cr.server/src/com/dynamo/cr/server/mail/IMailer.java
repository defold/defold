package com.dynamo.cr.server.mail;

import javax.mail.MessagingException;

public interface IMailer {

    void send(EMail email) throws MessagingException;

}
