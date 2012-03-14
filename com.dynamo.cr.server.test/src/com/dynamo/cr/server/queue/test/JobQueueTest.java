package com.dynamo.cr.server.queue.test;

import static org.junit.Assert.assertEquals;

import java.io.File;
import java.util.HashMap;

import javax.persistence.EntityManagerFactory;

import org.eclipse.persistence.config.PersistenceUnitProperties;
import org.eclipse.persistence.jpa.osgi.PersistenceProvider;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.server.queue.JobQueue;
import com.dynamo.cr.server.test.Util;
import com.google.common.base.Function;

public class JobQueueTest {

    private static final String PERSISTENCE_UNIT_NAME = "unit-test";
    private static EntityManagerFactory factory;

    @Before
    public void setUp() throws Exception {
        // "drop-and-create-tables" can't handle model changes correctly. We
        // need to drop all tables first.
        // Eclipse-link only drops tables currently specified. When the model
        // change the table set also change.
        File tmp_testdb = new File("tmp/testdb");
        if (tmp_testdb.exists()) {
            getClass().getClassLoader().loadClass(
                    "org.apache.derby.jdbc.EmbeddedDriver");
            Util.dropAllTables();
        }

        HashMap<String, Object> props = new HashMap<String, Object>();
        props.put(PersistenceUnitProperties.CLASSLOADER, this.getClass()
                .getClassLoader());
        // NOTE: JPA-PersistenceUnits: unit-test in plug-in MANIFEST.MF has to
        // be set. Otherwise the persistence unit is not found.
        factory = new PersistenceProvider().createEntityManagerFactory(
                PERSISTENCE_UNIT_NAME, props);
    }

    @After
    public void tearDown() {
        // NOTE: Important to close the factory if the tables should be dropped
        // and created in setUp above (drop-and-create-tables in
        // persistance.xml)
        factory.close();
    }

    static class Sum implements Function<byte[], Void> {
        int sum = 0;
        int delay;

        public Sum() {
            this.delay = 0;
        }

        public Sum(int delay) {
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

    static class AlwaysFail implements Function<byte[], Void> {
        @Override
        public Void apply(byte[] arg) {
            throw new RuntimeException("Failed");
        }
    }

    @Test
    public void testAdd() throws Exception {
        int n = 100;
        JobQueue jobQueue = new JobQueue(factory, "add", 1000, 1);
        for (int i = 0; i < n; ++i) {
            jobQueue.newJob(Integer.toString(i).getBytes());
        }

        Sum sum = new Sum();
        // Process queue
        jobQueue.process(sum);
        // Process again. Should be empty now
        jobQueue.process(sum);
        assertEquals((n - 1) * (n / 2), sum.sum);
    }

    @Test
    public void testMultiQueue() throws Exception {
        int n = 100;
        JobQueue jobQueue1 = new JobQueue(factory, "add1", 1000, 1);
        JobQueue jobQueue2 = new JobQueue(factory, "add2", 1000, 1);
        for (int i = 0; i < n; ++i) {
            jobQueue1.newJob(Integer.toString(i).getBytes());
            jobQueue2.newJob(Integer.toString(i * 10).getBytes());
        }
        Sum sum1 = new Sum();
        Sum sum2 = new Sum();
        // Process queue 1
        jobQueue1.process(sum1);
        // Process queue 2
        jobQueue2.process(sum2);
        assertEquals((n - 1) * (n / 2), sum1.sum);
        assertEquals((n - 1) * (n / 2) * 10, sum2.sum);
    }

    @Test
    public void testRetry1() throws Exception {
        int n = 4;
        JobQueue jobQueue = new JobQueue(factory, "add", 1000, 1);
        for (int i = 0; i < n; ++i) {
            jobQueue.newJob(Integer.toString(i).getBytes());
        }

        // Process queue, all fail
        jobQueue.process(new AlwaysFail());

        // Process again. minRetryDelay too large. Sum should be zero
        Sum sum = new Sum();
        jobQueue.process(sum);
        assertEquals(0, sum.sum);
    }

    @Test
    public void testRetry2() throws Exception {
        int n = 4;
        int minRetryDelay = 1;
        JobQueue jobQueue = new JobQueue(factory, "add", minRetryDelay, 2);
        for (int i = 0; i < n; ++i) {
            jobQueue.newJob(Integer.toString(i).getBytes());
        }

        // Process queue, all fail
        jobQueue.process(new AlwaysFail());

        // Sleep minRetryDelay
        Thread.sleep(minRetryDelay * 1000);

        // Process again. minRetryDelay set to 1 second. All jobs
        // should be processed now
        Sum sum = new Sum();
        jobQueue.process(sum);
        assertEquals((n - 1) * (n / 2), sum.sum);
    }

    @Test
    public void testMaxRetries() throws Exception {
        int n = 4;
        int minRetryDelay = 1;
        int maxRetries = 3;
        JobQueue jobQueue = new JobQueue(factory, "add", minRetryDelay,
                maxRetries);
        for (int i = 0; i < n; ++i) {
            jobQueue.newJob(Integer.toString(i).getBytes());
        }

        for (int i = 0; i < maxRetries; ++i) {
            assertEquals(n, jobQueue.getQueueLength());
            jobQueue.process(new AlwaysFail());
            Thread.sleep(minRetryDelay * 1000);
        }
        assertEquals(0, jobQueue.getQueueLength());
    }

    static class Producer extends Thread {
        private JobQueue jobQueue;
        private int n;
        private int delay;
        public Producer(JobQueue jobQueue, int n, int delay) {
            this.jobQueue = jobQueue;
            this.n = n;
            this.delay = delay;
        }

        @Override
        public void run() {
            for (int i = 0; i < n; ++i) {
                jobQueue.newJob(Integer.toString(i).getBytes());
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
        int n = 500;
        int producerDelay = 5;
        int consumerDelay = 10;
        JobQueue jobQueue = new JobQueue(factory, "add", 1000, 1);
        Producer producer = new Producer(jobQueue, n, producerDelay);
        producer.start();

        Sum sum = new Sum(consumerDelay);
        while (true) {
            jobQueue.process(sum);
            if (sum.sum == (n - 1) * (n / 2)) {
                break;
            }
        }
        producer.join();
        assertEquals((n - 1) * (n / 2), sum.sum);
    }

    @Test(timeout = 30000)
    public void testProducerConsumer2() throws Exception {
        int n = 500;
        int producerDelay = 10;
        int consumerDelay = 5;
        JobQueue jobQueue = new JobQueue(factory, "add", 1000, 1);
        Producer producer = new Producer(jobQueue, n, producerDelay);
        producer.start();

        Sum sum = new Sum(consumerDelay);
        while (true) {
            jobQueue.process(sum);
            if (sum.sum == (n - 1) * (n / 2)) {
                break;
            }
        }
        producer.join();
        assertEquals((n - 1) * (n / 2), sum.sum);
    }
}

