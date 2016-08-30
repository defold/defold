package com.dynamo.cr.server.queue;

import com.dynamo.cr.server.queue.ConstantBackoff;
import com.dynamo.cr.server.queue.ExponentialBackoff;
import com.dynamo.cr.server.queue.JobQueue;
import com.dynamo.cr.server.test.EntityManagerRule;
import com.google.common.base.Function;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;

import javax.persistence.EntityManager;
import javax.persistence.EntityManagerFactory;

import static org.junit.Assert.assertEquals;

public class JobQueueTest {

    @Rule
    public EntityManagerRule entityManagerRule = new EntityManagerRule();

    private EntityManagerFactory factory;

    @Before
    public void setUp() throws Exception {
        factory = entityManagerRule.getFactory();
    }

    private static class Sum implements Function<byte[], Void> {
        int sum = 0;
        int delay;

        Sum() {
            this.delay = 0;
        }

        Sum(int delay) {
            this.delay = delay;
        }

        @Override
        public Void apply(byte[] arg) {
            sum += Integer.parseInt(new String(arg));
            if (delay > 0) {
                try {
                    Thread.sleep(delay);
                } catch (InterruptedException e) {
                    throw new RuntimeException(e);
                }
            }
            return null;
        }
    }

    private static class AlwaysFail implements Function<byte[], Void> {
        int count;

        @Override
        public Void apply(byte[] arg) {
            ++count;
            throw new RuntimeException("Failed");
        }
    }

    private void process(JobQueue jobQueue, Function<byte[], Void> f) {
        EntityManager em = factory.createEntityManager();
        em.getTransaction().begin();
        jobQueue.process(em, f);
        em.getTransaction().commit();
        em.close();
    }

    private void newJob(JobQueue jobQueue, byte[] data) {
        EntityManager em = factory.createEntityManager();
        em.getTransaction().begin();
        jobQueue.newJob(em, data);
        em.getTransaction().commit();
        em.close();
    }

    private int queueLength(JobQueue jobQueue) {
        EntityManager em = factory.createEntityManager();
        try {
            return jobQueue.getQueueLength(em);
        } finally {
            em.close();
        }
    }

    @Test
    public void testAdd() throws Exception {
        int n = 100;
        JobQueue jobQueue = new JobQueue("add", new ConstantBackoff(1000), 1);
        for (int i = 0; i < n; ++i) {
            newJob(jobQueue, Integer.toString(i).getBytes());
        }

        Sum sum = new Sum();
        // Process queue
        process(jobQueue, sum);
        // Process again. Should be empty now
        process(jobQueue, sum);
        assertEquals((n - 1) * (n / 2), sum.sum);
    }

    @Test(timeout = 20000)
    public void testExponentialBackoff() throws Exception {
        int n = 4;
        ExponentialBackoff backoff = new ExponentialBackoff(2);
        JobQueue jobQueue = new JobQueue("add", backoff, 10);
        for (int i = 0; i < n; ++i) {
            newJob(jobQueue, Integer.toString(i).getBytes());
        }

        AlwaysFail fail = new AlwaysFail();
        long start = System.currentTimeMillis();
        // Backoff delays (power 2 set above)
        // sum(0 1 2 4) = 7
        while (System.currentTimeMillis() - start < (7+1) * 1000) {
            process(jobQueue, fail);
            Thread.sleep(50);
        }
        assertEquals(4 * n, fail.count);
    }

    @Test
    public void testMultiQueue() throws Exception {
        int n = 100;
        JobQueue jobQueue1 = new JobQueue("add1", new ConstantBackoff(1000), 1);
        JobQueue jobQueue2 = new JobQueue("add2", new ConstantBackoff(1000), 1);
        for (int i = 0; i < n; ++i) {
            newJob(jobQueue1, Integer.toString(i).getBytes());
            newJob(jobQueue2, Integer.toString(i * 10).getBytes());
        }
        Sum sum1 = new Sum();
        Sum sum2 = new Sum();
        // Process queue 1
        process(jobQueue1, sum1);
        // Process queue 2
        process(jobQueue2, sum2);
        assertEquals((n - 1) * (n / 2), sum1.sum);
        assertEquals((n - 1) * (n / 2) * 10, sum2.sum);
    }

    @Test
    public void testRetry1() throws Exception {
        int n = 4;
        JobQueue jobQueue = new JobQueue("add", new ConstantBackoff(1000), 1);
        for (int i = 0; i < n; ++i) {
            newJob(jobQueue, Integer.toString(i).getBytes());
        }

        // Process queue, all fail
        process(jobQueue, new AlwaysFail());

        // Process again. minRetryDelay too large. Sum should be zero
        Sum sum = new Sum();
        process(jobQueue, sum);
        assertEquals(0, sum.sum);
    }

    @Test
    public void testRetry2() throws Exception {
        int n = 4;
        int minRetryDelay = 1;
        JobQueue jobQueue = new JobQueue("add", new ConstantBackoff(minRetryDelay), 2);
        for (int i = 0; i < n; ++i) {
            newJob(jobQueue, Integer.toString(i).getBytes());
        }

        // Process queue, all fail
        process(jobQueue, new AlwaysFail());

        // Sleep minRetryDelay
        Thread.sleep(minRetryDelay * 1000);

        // Process again. minRetryDelay set to 1 second. All jobs
        // should be processed now
        Sum sum = new Sum();
        process(jobQueue, sum);
        assertEquals((n - 1) * (n / 2), sum.sum);
    }

    @Test
    public void testMaxRetries() throws Exception {
        int n = 4;
        int minRetryDelay = 1;
        int maxRetries = 3;
        JobQueue jobQueue = new JobQueue("add", new ConstantBackoff(minRetryDelay),
                maxRetries);
        for (int i = 0; i < n; ++i) {
            newJob(jobQueue, Integer.toString(i).getBytes());
        }

        for (int i = 0; i < maxRetries; ++i) {
            assertEquals(n, queueLength(jobQueue));
            process(jobQueue, new AlwaysFail());
            Thread.sleep(minRetryDelay * 1000);
        }
        assertEquals(0, queueLength(jobQueue));
    }

    private class Producer extends Thread {
        private JobQueue jobQueue;
        private int n;
        private int delay;

        Producer(JobQueue jobQueue, int n, int delay) {
            this.jobQueue = jobQueue;
            this.n = n;
            this.delay = delay;
        }

        @Override
        public void run() {
            for (int i = 0; i < n; ++i) {
                newJob(jobQueue, Integer.toString(i).getBytes());
                try {
                    Thread.sleep(delay);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            }
        }
    }

    @Test(timeout = 30000)
    public void testProducerConsumer1() throws Exception {
        int n = 100;
        int producerDelay = 5;
        int consumerDelay = 10;
        JobQueue jobQueue = new JobQueue("add", new ConstantBackoff(1000), 1);
        Producer producer = new Producer(jobQueue, n, producerDelay);
        producer.start();

        Sum sum = new Sum(consumerDelay);
        while (true) {
            process(jobQueue, sum);
            if (sum.sum == (n - 1) * (n / 2)) {
                break;
            }
        }
        producer.join();
        assertEquals((n - 1) * (n / 2), sum.sum);
    }

    @Test(timeout = 30000)
    public void testProducerConsumer2() throws Exception {
        int n = 100;
        int producerDelay = 10;
        int consumerDelay = 5;
        JobQueue jobQueue = new JobQueue("add", new ConstantBackoff(1000), 1);
        Producer producer = new Producer(jobQueue, n, producerDelay);
        producer.start();

        Sum sum = new Sum(consumerDelay);
        while (true) {
            process(jobQueue, sum);
            if (sum.sum == (n - 1) * (n / 2)) {
                break;
            }
        }
        producer.join();
        assertEquals((n - 1) * (n / 2), sum.sum);
    }
}

