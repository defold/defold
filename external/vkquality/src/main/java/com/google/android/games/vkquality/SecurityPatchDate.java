/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.google.android.games.vkquality;

public class SecurityPatchDate {
    public SecurityPatchDate(String SecurityPatch)
    {
        mYear = 0;
        mMonth = 0;
        mDay = 0;
        String[] patchParts = SecurityPatch.split("-");
        if (patchParts.length == 3)
        {
            try
            {
                mYear = Integer.parseInt(patchParts[0]);
                mMonth = Integer.parseInt(patchParts[1]);
                mDay = Integer.parseInt(patchParts[2]);
            }
            catch (NumberFormatException ignored)
            {
                // If any part is invalid, mark everything as 0
                mYear = 0;
                mMonth = 0;
                mDay = 0;
            }
        }
    }

    public boolean IsEqualOrLaterThan(SecurityPatchDate PatchDate)
    {
        if (mYear > PatchDate.mYear) return true;
        if (mYear == PatchDate.mYear)
        {
            if (mMonth > PatchDate.mMonth) return true;
            if (mMonth == PatchDate.mMonth)
            {
                return mDay >= PatchDate.mDay;
            }
        }
        return false;
    }

    public int getYear() { return mYear; }
    public int getMonth() { return mMonth; }
    public int getDay() { return mDay; }

    int mYear;
    int mMonth;
    int mDay;
}
