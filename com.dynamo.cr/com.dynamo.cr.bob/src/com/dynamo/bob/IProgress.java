package com.dynamo.bob;

import com.dynamo.bob.bundle.ICanceled;

public interface IProgress extends ICanceled {
    IProgress subProgress(int work);
    void worked(int amount);
    void beginTask(String name, int work);
    void done();
    boolean isCanceled();
}
