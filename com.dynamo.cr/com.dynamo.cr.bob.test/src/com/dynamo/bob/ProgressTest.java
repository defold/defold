// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
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

import org.junit.Test;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CopyOnWriteArrayList;

import static org.junit.Assert.*;

public class ProgressTest {
    private record Report(IProgress.Message message, double fraction) {
    }

    private static final class RecordingReporter implements Progress.Reporter {
        private final List<Report> reports = new CopyOnWriteArrayList<>();
        private boolean canceled;
        private int closeCalls;

        @Override
        public void report(IProgress.Message message, double fraction) {
            reports.add(new Report(message, fraction));
        }

        @Override
        public void close() {
            closeCalls++;
        }

        @Override
        public boolean isCanceled() {
            return canceled;
        }
    }

    @Test
    public void messageReportsWithoutAdvancingProgress() {
        var reporter = new RecordingReporter();
        try (var progress = new Progress(reporter)) {
            progress.message(IProgress.Message.Building.INSTANCE);
        }

        assertEquals(2, reporter.reports.size());
        assertSame(IProgress.Message.Building.INSTANCE, reporter.reports.get(0).message());
        assertEquals(0.0, reporter.reports.get(0).fraction(), 0.0);
        assertSame(IProgress.Message.Building.INSTANCE, reporter.reports.get(1).message());
        assertEquals(1.0, reporter.reports.get(1).fraction(), 0.0);
    }

    @Test
    public void nonPositiveWorkDoesNotReport() {
        var reporter = new RecordingReporter();
        try (var progress = new Progress(reporter)) {
            var split = progress.split(3);
            split.worked(0);
            split.worked(-1);
        }

        assertEquals(1, reporter.reports.size());
        assertSame(IProgress.Message.Working.INSTANCE, reporter.reports.get(0).message());
        assertEquals(1.0, reporter.reports.get(0).fraction(), 0.0);
    }

    @Test
    public void repeatedWorkedCallsReachExactCompletionWithoutLeftovers() {
        var reporter = new RecordingReporter();
        try (var progress = new Progress(reporter)) {
            var split = progress.split(3);
            split.worked();
            split.worked();
            split.worked();
        }

        assertEquals(3, reporter.reports.size());
        assertSame(IProgress.Message.Working.INSTANCE, reporter.reports.get(0).message());
        assertEquals(1.0 / 3.0, reporter.reports.get(0).fraction(), 1e-15);
        assertSame(IProgress.Message.Working.INSTANCE, reporter.reports.get(1).message());
        assertEquals(2.0 / 3.0, reporter.reports.get(1).fraction(), 1e-15);
        assertSame(IProgress.Message.Working.INSTANCE, reporter.reports.get(2).message());
        assertEquals(1.0, reporter.reports.get(2).fraction(), 0.0);
    }

    @Test
    public void manyLazySubtasksAllowAnyTwoClosuresToReachCompletion() {
        var reporter = new RecordingReporter();
        try (var root = new Progress(reporter)) {
            var split = root.split(2);
            var subtasks = new ArrayList<IProgress>();
            for (int i = 0; i < 256; i++) {
                subtasks.add(split.subtask());
            }

            subtasks.get(17).close();
            subtasks.get(193).close();
            for (var subtask : subtasks) {
                subtask.close();
            }
        }

        assertEquals(2, reporter.reports.size());
        assertEquals(0.5, reporter.reports.get(0).fraction(), 1e-15);
        assertEquals(1.0, reporter.reports.get(1).fraction(), 0.0);
    }

    @Test
    public void workedAndSubtaskCallsShareTheSameLazySplit() {
        var reporter = new RecordingReporter();
        try (var root = new Progress(reporter)) {
            var split = root.split(4);
            split.worked();
            try (var child = split.subtask(2)) {
                child.close();
            }
            split.worked();
        }

        assertEquals(3, reporter.reports.size());
        assertEquals(0.25, reporter.reports.get(0).fraction(), 1e-15);
        assertEquals(0.75, reporter.reports.get(1).fraction(), 1e-15);
        assertEquals(1.0, reporter.reports.get(2).fraction(), 0.0);
    }

    @Test
    public void nestedSplitsAndSubtasksReachCompletionWithoutDuplicateFullReports() {
        var reporter = new RecordingReporter();
        try (var root = new Progress(reporter)) {
            var rootSplit = root.split(2);
            try (var first = rootSplit.subtask();
                 var second = rootSplit.subtask()) {
                var firstSplit = first.split(2);
                try (var grandchild = firstSplit.subtask()) {
                    grandchild.close();
                }
                firstSplit.worked();
                first.close();
                second.close();
            }
        }

        assertEquals(3, reporter.reports.size());
        assertEquals(0.25, reporter.reports.get(0).fraction(), 1e-15);
        assertEquals(0.5, reporter.reports.get(1).fraction(), 1e-15);
        assertEquals(1.0, reporter.reports.get(2).fraction(), 0.0);
        for (int i = 1; i < reporter.reports.size(); i++) {
            assertTrue(reporter.reports.get(i - 1).fraction() <= reporter.reports.get(i).fraction());
        }
    }

    @Test
    public void oversizedWorkAndSubtasksClampToWhatRemains() {
        var reporter = new RecordingReporter();
        try (var root = new Progress(reporter)) {
            var split = root.split(2);
            try (var child = split.subtask(Long.MAX_VALUE)) {
                child.close();
            }
            split.worked(Long.MAX_VALUE);
            split.worked(Long.MAX_VALUE);
        }

        assertEquals(1, reporter.reports.size());
        assertEquals(1.0, reporter.reports.get(0).fraction(), 0.0);
    }

    @Test
    public void isCanceledDelegatesToReporter() {
        var reporter = new RecordingReporter();
        try (var progress = new Progress(reporter)) {
            assertFalse(progress.isCanceled());
            reporter.canceled = true;
            assertTrue(progress.isCanceled());
        }
    }

    @Test
    public void closeReportsCompletionAndClosesReporter() {
        var reporter = new RecordingReporter();
        var progress = new Progress(reporter);
        var split = progress.split(4);

        progress.message(IProgress.Message.Building.INSTANCE);
        split.worked(1);
        progress.close();

        assertEquals(3, reporter.reports.size());
        assertSame(IProgress.Message.Building.INSTANCE, reporter.reports.get(2).message());
        assertEquals(1.0, reporter.reports.get(2).fraction(), 0.0);
        assertEquals(1, reporter.closeCalls);
        assertFalse(progress.isCanceled());
    }

    @Test
    public void closeStopsFurtherNotifications() {
        var reporter = new RecordingReporter();
        var progress = new Progress(reporter);

        progress.close();
        progress.message(IProgress.Message.Cleaning.INSTANCE);
        progress.close();

        assertEquals(1, reporter.reports.size());
        assertSame(IProgress.Message.Working.INSTANCE, reporter.reports.get(0).message());
        assertEquals(1.0, reporter.reports.get(0).fraction(), 0.0);
        assertEquals(1, reporter.closeCalls);
    }

}
