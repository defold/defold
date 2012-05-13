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
    public JobQueue(String queue,
                    IBackoff backoff, int maxRetries) {
        this.queue = queue;
        this.backoff = backoff;
        this.maxRetries = maxRetries;
    }

    /**
     * Create a new job with data
     * @param em {@link EntityManager}
     * @param data job data
     */
    public void newJob(EntityManager em, byte[] data) {
        Job job = new Job(data, queue);
        em.persist(job);
    }

    /**
     * Process job queue.
     * @param em {@link EntityManager}
     * @param function job process function. Raise any exception to indicate failure.
     */
    public void process(EntityManager em, Function<byte[], Void> function) {
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
    }

    /**
     * Get current job queue length
     * @param em {@link EntityManager}
     * @return job queue length
     */
    public int getQueueLength(EntityManager em) {
        List<Job> list = em.createQuery("select j from Job j where j.queue = :queue", Job.class).setParameter("queue", queue).getResultList();
        return list.size();
    }
}

