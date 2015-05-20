package com.dynamo.cr.server.queue;

/**
 * Backoff delay function of number of retries
 * @author chmu
 *
 */
public interface IBackoff {

    /**
     * Get backup delay
     * @param retries number of retires performed
     * @return delay in seconds
     */
    int getDelay(int retries);
}
