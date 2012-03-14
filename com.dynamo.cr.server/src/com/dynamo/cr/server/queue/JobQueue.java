package com.dynamo.cr.server.queue;

import java.util.List;

import javax.persistence.EntityManager;
import javax.persistence.EntityManagerFactory;

import org.joda.time.DateTime;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.server.model.Job;
import com.google.common.base.Function;

public class JobQueue {

    private EntityManagerFactory factory;
    private String queue;
    private IBackoff backoff;
    private int maxRetries;
    private static Logger logger = LoggerFactory.getLogger(JobQueue.class);

    /**
     * Create a new job queue
     * @param factory {@link EntityManagerFactory}
     * @param queue queue name. must be unique, mail-queue
     * @param backoff backoff function. {@link IBackoff}
     * @param maxRetries maximum number of retries until the job is discarded
     */
    public JobQueue(EntityManagerFactory factory,
                    String queue,
                    IBackoff backoff, int maxRetries) {
        this.factory = factory;
        this.queue = queue;
        this.backoff = backoff;
        this.maxRetries = maxRetries;
    }

    /**
     * Create a new job with data
     * @param data job data
     */
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

    /**
     * Process job queue.
     * @param function job process function. Raise any exception to indicate failure.
     */
    public void process(Function<byte[], Void> function) {
        EntityManager em = factory.createEntityManager();
        try {
            em.getTransaction().begin();

            List<Job> list = em.createQuery("select j from Job j where j.queue = :queue", Job.class).setParameter("queue", queue).getResultList();
            for (Job job : list) {

                DateTime now = new DateTime();
                DateTime earliest = new DateTime(job.getEarliest());

                if (now.isAfter(earliest)) {
                    try {
                        // Process
                        function.apply(job.getData());
                        em.remove(job);
                    } catch (Throwable e) {
                        job.setEarliest(now.plusSeconds(backoff.getDelay(job.getRetries())).toDate());
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

    /**
     * Get current job queue length
     * @return job queue length
     */
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

