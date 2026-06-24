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

import android.os.Build;

import java.util.ArrayList;
import java.util.List;

public class StartupMitigation {
    public static final int DEVICE_UNAFFECTED = 0;
    public static final int DEVICE_AFFECTED = 1;

    public StartupMitigation()
    {
        mApiLevel = Build.VERSION.SDK_INT;
        mBrand = Build.BRAND;
        mDevice = Build.DEVICE;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            mSoC = Build.SOC_MODEL;
        }
        else
        {
            mSoC = "";
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            mSecurityPatchDate = new SecurityPatchDate(Build.VERSION.SECURITY_PATCH);
        } else {
            // Fallback to a safe old security patch date if we can't query it
            // from the OS
            mSecurityPatchDate = new SecurityPatchDate("2021-01-01");
        }
        mStartupRecordList = new ArrayList<StartupMitigationRecord>();
        mUseVulkan = false;
        mResultString = "StartupMitigation not yet run";
    }

    public void createMitigationDeviceList()
    {
        // Device and SoC information is currently hardcoded into this function, if further
        // expansion is warranted, it will be serialized from a resource file
        SecurityPatchDate genericVulkanDate = new SecurityPatchDate("2024-06-01");
        // Date used to specify 'unreachable'/'unfixed'
        SecurityPatchDate genericUnfixedDate = new SecurityPatchDate("2099-12-31");

        // Create some generic SoC entries, but since SOC_MODEL requires S or higher,
        // also make device specific entries for known-affected models, the SoC
        // compare is a substring contains, since sometimes these report as QTI SM8650 and
        // sometimes just SM8650
        StartupMitigationRecord generic8650 = new StartupMitigationRecord("samsung",
                "", "SM8650", 34, 99, genericUnfixedDate, genericVulkanDate);
        mStartupRecordList.add(generic8650);

        StartupMitigationRecord generic8550 = new StartupMitigationRecord("samsung",
                "", "SM8550", 34, 99, genericUnfixedDate, genericVulkanDate);
        mStartupRecordList.add(generic8550);

        StartupMitigationRecord generic8475 = new StartupMitigationRecord("samsung",
                "", "SM8475", 34, 99, genericUnfixedDate, genericVulkanDate);
        mStartupRecordList.add(generic8475);

        StartupMitigationRecord generic8450 = new StartupMitigationRecord("samsung",
                "", "SM8450", 34, 99, genericUnfixedDate, genericVulkanDate);
        mStartupRecordList.add(generic8450);

        // Don't recommend Vulkan on the 6375
        StartupMitigationRecord generic6375 = new StartupMitigationRecord("samsung",
                "", "SM6375", 34, 99, genericUnfixedDate, genericUnfixedDate);
        mStartupRecordList.add(generic6375);

        ArrayList<String> vulkanDevices = getVulkanDeviceStrings();
        for (String vulkanDevice : vulkanDevices)
        {
            StartupMitigationRecord addVulkanDevice = new StartupMitigationRecord("samsung",
                    vulkanDevice, "", 34, 99, genericUnfixedDate, genericVulkanDate);
            mStartupRecordList.add(addVulkanDevice);
        }

        ArrayList<String> glesDevices = new ArrayList<String>();
        glesDevices.add("a23xq");
        glesDevices.add("gta9pwifi");
        for (String glesDevice : glesDevices)
        {
            StartupMitigationRecord addGlesDevice = new StartupMitigationRecord("samsung",
                    glesDevice, "", 34, 99, genericUnfixedDate, genericUnfixedDate);
            mStartupRecordList.add(addGlesDevice);
        }
    }

    private static ArrayList<String> getVulkanDeviceStrings() {
        ArrayList<String> vulkanDevices = new ArrayList<String>();
        vulkanDevices.add("e3q");
        vulkanDevices.add("b0q");
        vulkanDevices.add("dm3q");
        vulkanDevices.add("r0q");
        vulkanDevices.add("e2q");
        vulkanDevices.add("g0q");
        vulkanDevices.add("dm1q");
        vulkanDevices.add("q4q");
        vulkanDevices.add("e1q");
        vulkanDevices.add("dm2q");
        vulkanDevices.add("q5q");
        vulkanDevices.add("r11q");
        vulkanDevices.add("b4q");
        vulkanDevices.add("b5q");
        vulkanDevices.add("gts8wifi");
        vulkanDevices.add("SC-51C");
        vulkanDevices.add("gts8p");
        return vulkanDevices;
    }

    public int isDeviceAffected()
    {
        // If this list actually gets large, refactor to a dictionary lookup
        for (StartupMitigationRecord record : mStartupRecordList)
        {
            if (record.RecordMatch(mBrand, mDevice, mSoC))
            {
                mUseVulkan = record.RecommendAffectedVulkan(mSecurityPatchDate);
                int result = record.IsDeviceAffected(mApiLevel, mSecurityPatchDate);
                if (result == DEVICE_AFFECTED)
                {
                    // verbose version
                    mResultString = "Startup mitigation: Device found in mitigation list, affected"
                            + " brand: " + mBrand
                            + " device: " + mDevice
                            + " SoC:" + mSoC
                            + " useVulkan: " + mUseVulkan;
//                    mResultString = "Startup mitigation: Device found in mitigation list, affected" +
//                            " useVulkan: " + mUseVulkan;
                }
                else
                {
                    mResultString = "Startup mitigation: Device found in mitigation list, unaffected";
                }
                return result;
            }
        }
        mResultString = "Startup mitigation: Device not found in mitigation list, unaffected";
        return DEVICE_UNAFFECTED;
    }

    boolean useVulkanOnAffectedDevice()
    {
        return mUseVulkan;
    }

    // For logcat reporting
    String getResultString() { return mResultString; }

    int mApiLevel;
    String mBrand;
    String mDevice;
    String mSoC;
    SecurityPatchDate mSecurityPatchDate;
    List<StartupMitigationRecord> mStartupRecordList;
    boolean mUseVulkan;
    String mResultString;
}
