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
