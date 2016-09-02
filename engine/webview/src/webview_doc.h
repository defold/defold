/*# Webview API documentation
 *
 * Functions and for creating and controlling webviews to show html pages
 * or evaluate javascript.
 *
 * @name Webview
 * @namespace webview
 */


/*# Creates a webview

 * Creates a webview instance. It that can show html pages as well as evaluate javascript. It remains hidden until the first call
 * 
 * @name webview.create
 *
 * @param callback (function) A callback which receives info about finished requests.
 * 
 * <ul>
 *     <li>self (object) The calling script</li>
 *     <li>webview_id (number) The webview id</li>
 *     <li>request_id (number) The request id</li>
 *     <li>type (number) The type of the callback. Can be one of these:</li>
 *     <ul>
 *         <li>webview.CALLBACK_RESULT_URL_OK</li>
 *         <li>webview.CALLBACK_RESULT_URL_ERROR</li>
 *         <li>webview.CALLBACK_RESULT_EVAL_OK</li>
 *         <li>webview.CALLBACK_RESULT_EVAL_ERROR</li>
 *     </ul>
 *     <li>data (table) The callback value ''data'' is a table which currently holds these values</li>
 *     <ul>
 *         <li>url (string) The url used in the webview.open() call. nil otherwise.</li>
 *         <li>result (string) Holds the result of either: a failed url open, a successful eval request or a failed eval. nil otherwise</li>
 *     </ul>
 * </ul>
 *
 * @return (int) The id number of the webview
 *
 * @note On iOS, the callback will never get a webview.CALLBACK_RESULT_EVAL_ERROR, due to the iOS SDK implementation
 *
 * @examples
 * <pre>
 * function webview_callback(self, webview_id, request_id, type, data)
 *     if type == webview.CALLBACK_RESULT_URL_OK then
 *         -- the page is now loaded, let's show it
 *         webview.set_visible(webview_id, 1)
 *     elseif type == webview.CALLBACK_RESULT_URL_ERROR then
 *         print("Failed to load url: " .. data["url"])
 *         print("Error: " .. data["error"])
 *     elseif type == webview.CALLBACK_RESULT_EVAL_OK then
 *         print("Eval ok. Result: " .. data['result'])
 *     elseif type == webview.CALLBACK_RESULT_EVAL_ERROR then
 *         print("Eval not ok. Request # " .. request_id)
 *     end
 * end
 * 
 * local webview_id = webview.create(webview_callback)
 * </pre>
 */


/*# Destroys a webview
 *
 * Destroys an instance of a webview
 * 
 * @name webview.destroy
 *
 * @param webview_id (number) The webview id (returned by the webview.create() call)
 */

/*# Open a page uring an url
 * 
 * Opens a web page in the webview, using an url. Once the request is done, the callback (registered in webview.create() ) is invoked
 * 
 * @name webview.open
 *
 * @param webview_id    (number) The webview id
 * @param url           (string) The url to open
 * @param options       (table) A table of options for the request. Currently it holds these options
 * <ul>
 *     <li>hidden (boolean) If true, the webview will stay hidden (default=false)</li>
 * </ul>
 *
 * @return (int) The id number of the request
 *
 * @examples
 * <pre>
 * local request_id = webview.open(webview_id, "http://www.defold.com", {hidden = true})
 * </pre>
 */


/*# Open a page using html
 * 
 * Opens a web page in the webview, using html data. Once the request is done, the callback (registered in webview.create() ) is invoked
 * 
 * @name webview.open_raw
 *
 * @param webview_id    (number) The webview id
 * @param html          (string) The html data to display
 * @param options       (table) A table of options for the request. See webview.open()
 *
 * @return (int) The id number of the request
 *
 * @examples
 * <pre>
 * local html = sys.load_resource("/main/data/test.html")
 * local request_id = webview.open_raw(webview_id, html, {hidden = true})
 * </pre>
 */


/*# Evaluates javascript in a webview
 * 
 * Evaluates java script within the context of the currently loaded page (if any). Once the request is done, the callback (registered in webview.create() ) is invoked. The callback will get the result in the data["result"] field.
 * 
 * @name webview.eval
 *
 * @param webview_id    (number) The webview id
 * @param code          (string) The java script code to evaluate
 *
 * @return (int) The id number of the request
 *
 * @examples
 * <pre>
 * local request_id = webview.eval(webview_id, "GetMyFormData()")
 * </pre>
 */

/*# Shows or hides a web view
 * 
 * Shows or hides a web view
 * 
 * @name webview.set_visible
 *
 * @param webview_id    (number) The webview id (returned by the webview.create() call)
 * @param visible       (number) If 0, hides the webview. If non zero, shows the view
 */


/*# Gets the visibility state of the webview
 * 
 * Gets the visibility state of the webview
 * 
 * @name webview.is_visible
 *
 * @param webview_id    (number) The webview id (returned by the webview.create() call)
 * @return              (number) Returns 0 if not visible, 1 if it is visible
 */










