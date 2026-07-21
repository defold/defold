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

import com.dynamo.bob.bundle.ICanceled;

import java.net.URI;

/// Reports progress for a unit of work
public interface IProgress extends ICanceled, AutoCloseable {

    /// Updates the current message.
    ///
    /// @param message message to report
    void message(Message message);

    /// Mark this progress scope as complete
    @Override
    void close();

    /// Splits this progress scope into `parts` logical units.
    ///
    /// @param parts total number of parts in this split
    /// @return split handle for reporting work within this scope
    ISplit split(long parts);

    /// A shallow view over a progress scope split into parts.
    ///
    /// Intended to stay within the lexical scope where it was created. If work is delegated
    /// to another function, pass an `IProgress` subtask instead of the `ISplit`.
    interface ISplit {
        /// Marks `parts` parts as done.
        ///
        /// @param parts number of parts completed
        void worked(long parts);

        /// Creates a nested subtask that completest `parts` parts from this split.
        ///
        /// @param parts number of split parts reserved for the subtask
        /// @return nested progress scope
        IProgress subtask(long parts);

        /// Creates a nested subtask that completes at 1 part from this split.
        ///
        /// @return nested progress scope
        default IProgress subtask() {
            return subtask(1L);
        }

        /// Marks 1 part as done.
        default void worked() {
            worked(1L);
        }
    }

    /// Marker type for progress messages reported by `IProgress`
    sealed interface Message permits IProgress.Message.Building, IProgress.Message.BuildingEngine, IProgress.Message.Bundling, IProgress.Message.Cleaning, IProgress.Message.CleaningEngine, IProgress.Message.DownloadingArchive, IProgress.Message.DownloadingArchives, IProgress.Message.DownloadingSymbols, IProgress.Message.GeneratingReport, IProgress.Message.ReadingClasses, IProgress.Message.ReadingTasks, IProgress.Message.TranspilingToLua, IProgress.Message.Working {
        record Bundling() implements IProgress.Message {
            public static final Bundling INSTANCE = new Bundling();
        }

        record BuildingEngine() implements IProgress.Message {
            public static final BuildingEngine INSTANCE = new BuildingEngine();
        }

        record CleaningEngine() implements IProgress.Message {
            public static final CleaningEngine INSTANCE = new CleaningEngine();
        }

        record DownloadingSymbols() implements IProgress.Message {
            public static final DownloadingSymbols INSTANCE = new DownloadingSymbols();
        }

        record TranspilingToLua() implements IProgress.Message {
            public static final TranspilingToLua INSTANCE = new TranspilingToLua();
        }

        record ReadingTasks() implements IProgress.Message {
            public static final ReadingTasks INSTANCE = new ReadingTasks();
        }

        record Building() implements IProgress.Message {
            public static final Building INSTANCE = new Building();
        }

        record Cleaning() implements IProgress.Message {
            public static final Cleaning INSTANCE = new Cleaning();
        }

        record GeneratingReport() implements IProgress.Message {
            public static final GeneratingReport INSTANCE = new GeneratingReport();
        }

        record Working() implements IProgress.Message {
            public static final Working INSTANCE = new Working();
        }

        record ReadingClasses() implements IProgress.Message {
            public static final ReadingClasses INSTANCE = new ReadingClasses();
        }

        record DownloadingArchives(int count) implements IProgress.Message {
        }

        record DownloadingArchive(URI uri) implements IProgress.Message {
        }
    }
}
