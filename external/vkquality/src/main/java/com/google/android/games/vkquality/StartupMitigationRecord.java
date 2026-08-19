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

import static com.google.android.games.vkquality.StartupMitigation.DEVICE_AFFECTED;
import static com.google.android.games.vkquality.StartupMitigation.DEVICE_UNAFFECTED;

public class StartupMitigationRecord extends DeviceMitigationRecord {
    public StartupMitigationRecord(String Brand, String Device, String SoC,
                                   int AffectedApi,
                                   int FixedApi,
                                   SecurityPatchDate FixedSecurityPatch,
                                   SecurityPatchDate VulkanSecurityPatch)
    {
        super(Brand, Device, SoC);
        mAffectedApi = AffectedApi;
        mFixedApi = FixedApi;
        mFixedSecurityPatch = FixedSecurityPatch;
        mVulkanSecurityPatch = VulkanSecurityPatch;
    }

    public int IsDeviceAffected(int ApiLevel, SecurityPatchDate SecurityPatch)
    {
        if (ApiLevel >= mFixedApi) return DEVICE_UNAFFECTED;
        if (ApiLevel <= mAffectedApi)
        {
            if (SecurityPatch.IsEqualOrLaterThan(mFixedSecurityPatch)) return DEVICE_UNAFFECTED;
        }
        return DEVICE_AFFECTED;
    }

    public boolean RecommendAffectedVulkan(SecurityPatchDate SecurityPatch)
    {
        return SecurityPatch.IsEqualOrLaterThan(mVulkanSecurityPatch);
    }

    int mAffectedApi;
    int mFixedApi;
    SecurityPatchDate mFixedSecurityPatch;
    SecurityPatchDate mVulkanSecurityPatch;
}
