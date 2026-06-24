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

public class DeviceMitigationRecord {
    public DeviceMitigationRecord(String Brand, String Device, String SoC)
    {
        Brand_ = Brand;
        Device_ = Device;
        SoC_ = SoC;
    }

    public boolean RecordMatch(String Brand, String Device, String SoC)
    {
        // If Device is valid, compare it first, otherwise fallback
        // to comparing the SoC. If Brand_ is empty, treat as wildcard
        if (!Device.isEmpty() && !Device_.isEmpty()) {
            if (Brand_.isEmpty())
            {
               return Device.equals(Device_);
            }
            else
            {
                return (Brand.equals(Brand_) && Device.equals(Device_));
            }
        }
        if (Brand_.isEmpty()) {
            return SoC.contains(SoC_);
        }
        return (Brand.equals(Brand_) && SoC.contains(SoC_));
    }

    private String Brand_;
    private String Device_;
    private String SoC_;
}
