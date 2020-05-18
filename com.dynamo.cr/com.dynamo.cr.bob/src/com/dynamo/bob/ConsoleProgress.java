// Copyright 2020 The Defold Foundation
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
// 
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
// 
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

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