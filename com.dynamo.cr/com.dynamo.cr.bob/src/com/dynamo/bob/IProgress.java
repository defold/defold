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

public interface IProgress extends ICanceled {
    IProgress subProgress(int work);
    void worked(int amount);
    void beginTask(Task name, int work);
    void done();
    boolean isCanceled();

    enum Task {
        BUNDLING,
        BUILDING_ENGINE,
        CLEANING_ENGINE,
        DOWNLOADING_SYMBOLS,
        TRANSPILING_TO_LUA,
        READING_TASKS,
        BUILDING,
        CLEANING,
        GENERATING_REPORT,
        WORKING,
        READING_CLASSES,
        DOWNLOADING_ARCHIVES
    }
}
