#ifndef WINDOW_PRIVATE_H
#define WINDOW_PRIVATE_H

#include <script/script.h>


namespace dmWindow
{

enum Event
{
    EVENT_FOCUS_LOST = 0,
    EVENT_FOCUS_GAINED = 1,
    EVENT_RESIZED = 2,
};

}

#endif


/*# Sets a window event listener
 * Sets a window event listener.
 * 
 * @name window.set_listener
 *
 * @param callback (function) A callback which receives info about window events. Can be nil.
 * 
 * <ul>
 *     <li>self (object) The calling script</li>
 *     <li>event (number) The type of event. Can be one of these:</li>
 *     <ul>
 *         <li>window.EVENT_FOCUS_LOST</li>
 *         <li>window.EVENT_FOCUS_GAINED</li>
 *         <li>window.EVENT_RESIZED</li>
 *     </ul>
 *     <li>data (table) The callback value ''data'' is a table which currently holds these values</li>
 *     <ul>
 *         <li>width (number) The width of a resize event. nil otherwise.</li>
 *         <li>height (number) The height of a resize event. nil otherwise.</li>
 *     </ul>
 * </ul>
 *
 * @examples
 * <pre>
 * function window_callback(self, event, data)
 *     if event == window.EVENT_FOCUS_LOST then
 *         -- the game should pause, or consume less resources
 *     elseif type == window.EVENT_FOCUS_GAINED then
 *         -- the game should run again
 *     elseif type == window.EVENT_RESIZED then
 *         -- the window was resized
 *         print("Window size:", data.width, data.height)
 *     end
 * end
 * 
 * window.set_listener(window_callback)
 * </pre>
 */
