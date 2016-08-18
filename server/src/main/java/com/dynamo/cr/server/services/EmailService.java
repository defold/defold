package com.dynamo.cr.server.services;

import com.dynamo.cr.proto.Config;
import com.dynamo.cr.server.mail.EMail;
import com.dynamo.cr.server.mail.IMailProcessor;

import javax.inject.Inject;
import javax.persistence.EntityManager;
import java.util.Map;

public class EmailService {
    private final IMailProcessor mailProcessor;
    private final EntityManager entityManager;

    @Inject
    public EmailService(IMailProcessor mailProcessor, EntityManager entityManager) {
        this.mailProcessor = mailProcessor;
        this.entityManager = entityManager;
    }

    void send(String email, Config.EMailTemplate template, Map<String, String> params) {
        EMail emailMessage = EMail.format(template, email, params);
        mailProcessor.send(entityManager, emailMessage);
    }
}
