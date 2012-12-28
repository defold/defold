package com.dynamo.bob;

public class NullProgress implements IProgress {

    @Override
    public IProgress subProgress(int work) {
        return new NullProgress();
    }

    @Override
    public void worked(int amount) {
    }

    @Override
    public void beginTask(String name, int work) {
    }

    @Override
    public void done() {
    }

}