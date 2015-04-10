package com.dynamo.cr.server.queue;

/**
 * Constant backoff delay function. The retry-count is ignored.
 * @author chmu
 *
 */
public class ConstantBackoff implements IBackoff {

    private int delay;

    public ConstantBackoff(int delay) {
        this.delay = delay;
    }

    @Override
    public int getDelay(int retries) {
        return delay;
    }
}
