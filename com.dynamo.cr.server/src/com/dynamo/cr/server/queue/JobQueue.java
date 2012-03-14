package com.dynamo.cr.server.queue;

import java.util.List;

import javax.persistence.EntityManager;
import javax.persistence.EntityManagerFactory;

import org.joda.time.DateTime;
import org.joda.time.Duration;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.server.model.Job;
import com.google.common.base.Function;

public class JobQueue {

    private EntityManagerFactory factory;
    private String queue;
    private int minRetryDelay;
    private int maxRetries;
    private static Logger logger = LoggerFactory.getLogger(JobQueue.class);

    public JobQueue(EntityManagerFactory factory,
                    String queue,
                    int minRetryDelay, int maxRetries) {
        this.factory = factory;
        this.queue = queue;
        this.minRetryDelay = minRetryDelay;
        this.maxRetries = maxRetries;
    }

    public void newJob(byte[] data) {
        Job job = new Job(data, queue);
        EntityManager em = factory.createEntityManager();

        try {
            em.getTransaction().begin();
            em.persist(job);
            em.getTransaction().commit();
        } finally {
            em.close();
        }
    }

    public void process(Function<byte[], Void> function) {
        EntityManager em = factory.createEntityManager();
        try {
            em.getTransaction().begin();

            List<Job> list = em.createQuery("select j from Job j where j.queue = :queue", Job.class).setParameter("queue", queue).getResultList();
            for (Job job : list) {

                DateTime now = new DateTime();
                DateTime lastProcessed = new DateTime(job.getLastProcessed());
                Duration duration = new Duration(lastProcessed, now);
                if (duration.getStandardSeconds() >= minRetryDelay) {
                    try {
                        // Process
                        function.apply(job.getData());
                        em.remove(job);
                    } catch (Throwable e) {
                        job.setLastProcessed(now.toDate());
                        job.setRetries(job.getRetries() + 1);
                        if (job.getRetries() >= maxRetries) {
                            logger.warn("Removing job {} after {} retries", job.toString(), job.getRetries());
                            em.remove(job);
                        } else {
                            em.persist(job);
                        }

                        logger.warn("Job {} failed", job.toString(), e);
                    }
                }
            }

            em.getTransaction().commit();
        } finally {
            em.close();
        }
    }

    public int getQueueLength() {
        EntityManager em = factory.createEntityManager();
        try {
            List<Job> list = em.createQuery("select j from Job j where j.queue = :queue", Job.class).setParameter("queue", queue).getResultList();
            return list.size();
        } finally {
            em.close();
        }
    }
}

