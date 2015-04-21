package com.dynamo.cr.server.mail;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.ObjectInputStream;

import javax.inject.Inject;
import javax.mail.MessagingException;
import javax.persistence.EntityManager;
import javax.persistence.EntityManagerFactory;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.server.queue.ExponentialBackoff;
import com.dynamo.cr.server.queue.JobQueue;
import com.google.common.base.Function;

public class MailProcessor implements Runnable, IMailProcessor {

    protected static Logger logger = LoggerFactory.getLogger(MailProcessor.class);

    private volatile boolean run = true;
    private IMailer mailer;
    private EntityManagerFactory factory;
    private int updateInterval;
    private Thread thread;
    private JobQueue jobQueue;

    @Inject
    public MailProcessor(IMailer mailer, EntityManagerFactory factory) {
        this.mailer = mailer;
        this.factory = factory;
        this.updateInterval = 5; // TODO: Currently hard-coded
        // Retry 8 times for ~12 hours
        jobQueue = new JobQueue("mail-queue", new ExponentialBackoff(6), 8);
    }

    @Override
    public void run() {
        while (run) {
            EntityManager em = factory.createEntityManager();
            try {
                em.getTransaction().begin();
                processQueue(em);
                em.getTransaction().commit();
            } finally {
                em.close();
            }
            try {
                Thread.sleep(updateInterval * 1000);
            } catch (InterruptedException e) {
            }
        }
    }

    @Override
    public void process() {
        this.thread.interrupt();
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.server.mail.IMailProcessor#send(javax.persistence.EntityManager, com.dynamo.cr.server.mail.EMail)
     */
    @Override
    public void send(EntityManager em, EMail email) {
        jobQueue.newJob(em, email.serialize());
    }

    private void processQueue(EntityManager em) {
        jobQueue.process(em, new Function<byte[], Void>() {
            @Override
            public Void apply(byte[] arg) {
                try {
                    ObjectInputStream os = new ObjectInputStream(new ByteArrayInputStream(arg));
                    EMail email = (EMail) os.readObject();
                    mailer.send(email);
                } catch (IOException e) {
                    throw new RuntimeException(e);
                } catch (ClassNotFoundException e) {
                    throw new RuntimeException(e);
                } catch (MessagingException e) {
                    throw new RuntimeException(e);
                }
                return null;
            }
        });
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.server.mail.IMailProcessor#start()
     */
    @Override
    public void start() {
        thread = new Thread(this);
        thread.start();
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.server.mail.IMailProcessor#stop()
     */
    @Override
    public void stop() {
        run = false;
        thread.interrupt();
        try {
            thread.join();
        } catch (InterruptedException e) {
            logger.error("", e);
        }
    }

}
