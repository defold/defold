package com.dynamo.cr.server.mail;

import javax.persistence.EntityManager;

public interface IMailProcessor {

    void send(EntityManager em, EMail email);

    void start();

    void stop();

    void process();

}