package com.dynamo.cr.server.queue;

/**
 * Exponential backoff function, base^retries
 * For base two the delay sequence will be
 * 1 2 4... seconds
 * but the first initial attempt excluded
 * The effective delay sequence will be
 * 0 1 2 4...
 * @author chmu
 *
 */
public class ExponentialBackoff implements IBackoff {

    private int base;

    public ExponentialBackoff(int base) {
        this.base = base;
    }

    @Override
    public int getDelay(int retries) {
        return (int) Math.pow(base, retries);
    }
}
