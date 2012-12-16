package com.dynamo.bob;

public interface IProgress {

    public IProgress subProgress(int work);
    public void worked(int amount);
    public void beginTask(String name, int work);
    public void done();

}
