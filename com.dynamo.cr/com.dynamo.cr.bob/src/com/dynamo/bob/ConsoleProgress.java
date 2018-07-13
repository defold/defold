package com.dynamo.bob;

public class ConsoleProgress implements IProgress {

    private float totalWork;
    private int prevPercent;
    private float worked;
    private ConsoleProgress reportTo;
    private float ticks;
    private float scale = 1;
    private Boolean isATTY = false;

    public ConsoleProgress() {
        reportTo = this;
    }

    private ConsoleProgress(ConsoleProgress parent, float ticks) {
        this.reportTo = parent;
        this.ticks = ticks;
        this.isATTY = System.console() != null;
        this.prevPercent = -1;
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
    }

    private void workedInternal(float amount) {
        this.worked += amount;
        printProgress();
    }

    private void printProgress() {
        int percent = (int)(100 * worked / totalWork);
        if (percent == prevPercent)
            return;

        if (this.isATTY) {
            System.out.print("\r                               \r");
        } else {
            if (worked > 0)
                System.out.print(" ");
            else
                System.out.print(", ");
        }

        String s = String.format("%d%%", percent);
        System.out.print(s);

        prevPercent = percent;
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