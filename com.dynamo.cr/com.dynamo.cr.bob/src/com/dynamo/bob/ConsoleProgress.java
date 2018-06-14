package com.dynamo.bob;

public class ConsoleProgress implements IProgress {

    private float EPSILON = 0.001f;
    private float totalWork;
    private float prev_worked;
    private float worked;
    private ConsoleProgress reportTo;
    private float ticks;
    private float scale = 1;

    public ConsoleProgress() {
        reportTo = this;
    }

    private ConsoleProgress(ConsoleProgress parent, float ticks) {
        this.reportTo = parent;
        this.ticks = ticks;
    }

    @Override
    public IProgress subProgress(int work) {
        return new ConsoleProgress(this, work);
    }

    @Override
    public void worked(int amount) {
        float report = Math.min(amount, this.totalWork - worked);
        if (reportTo != this) {
            this.worked += report;
        }
        if (scale > 0) {
            this.reportTo.workedInternal(report / scale);
        }
        prev_worked = worked;
    }

    private void workedInternal(float amount) {
        this.worked += amount;
        printProgress();
    }

    private void printProgress() {
        if (worked - prev_worked > EPSILON) {
            System.out.print("\r                               \r");
            String s = String.format("%.0f%%", 100 * worked / totalWork);
            System.out.print(s);
        }
    }

    @Override
    public void beginTask(String name, int work) {
        if (reportTo != this) {
            this.scale = work / ticks;
        }
        this.totalWork = work;
    }

    @Override
    public void done() {
        worked((int) totalWork); // Ensure 100%
        if (reportTo == this) {
            printProgress();
            System.out.println();
        }
    }

    @Override
    public boolean isCanceled() {
        return false;
    }

}