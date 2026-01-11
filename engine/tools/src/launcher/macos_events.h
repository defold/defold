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

#ifndef DM_MACOS_EVENTS_H
#define DM_MACOS_EVENTS_H

/**
 * Get a list of all paths sent by an openFiles event; see:
 * https://developer.apple.com/documentation/appkit/nsapplicationdelegate/1428742-application?language=objc
 * The OS will send this event to an app if files were dropped on the app icon
 * or if an associated file was double-clicked.
 *
 * To receive openFiles events that techically occurred before the app was
 * launched this function must be called from the applicaiton main funciton.
 * @return A NULL terminated list of utf-8 encoded, NULL terminated strings on
 * success. Or NULL if there was no event to receive. Call {@link #FreeFileList}
 * to release resources associated with the returned list.
 */
char** ReceiveFileOpenEvent();

/**
 * Release resources associated with a file list returned by {@link
 * #ReceiveFileOpenEvent}.
 * @param fileList list to free.
 */
void FreeFileList(char** fileList);

#endif
