/*# Webview API documentation
 *
 * Functions and for creating and controlling webviews to show html pages
 * or evaluate javascript. These API:s only exist on mobile platforms.
 * [icon:ios] [icon:android]
 *
 * @document
 * @name Webview
 * @namespace webview
 */

/*# Creates a webview

 * Creates a webview instance. It can show HTML pages as well as evaluate Javascript.
 * The view remains hidden until the first call.
 *
 * [icon:ios] On iOS, the callback will never get a webview.CALLBACK_RESULT_EVAL_ERROR,
 * due to the iOS SDK implementation.
 *
 * @name webview.create
 * @param callback [type:function(self, webview_id, request_id, type, data)] A callback which receives info about finished requests taking the following parameters
 *
 * `self`
 * : [type:object] The calling script
 *
 * `webview_id`
 * : [type:number] The webview id
 *
 * `request_id`
 * : [type:number] The request id
 *
 * `type`
 * : [type:number] The type of the callback. Can be one of these:
 *
 * - `webview.CALLBACK_RESULT_URL_OK`
 * - `webview.CALLBACK_RESULT_URL_ERROR`
 * - `webview.CALLBACK_RESULT_EVAL_OK`
 * - `webview.CALLBACK_RESULT_EVAL_ERROR`
 *
 * `data`
 * : [type:table] A table holding the data. The table has these fields:
 *
 * - [type:string] `url`: The url used in the webview.open() call. `nil` otherwise.
 * - [type:string] `result`: Holds the result of either: a failed url open, a successful eval request or a failed eval. `nil` otherwise
 *
 * @return id [type:number] The id number of the webview
 *
 * @examples
 *
 * ```lua
 * local function webview_callback(self, webview_id, request_id, type, data)
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
 * ```
 */

/*# Destroys a webview
 *
 * Destroys an instance of a webview.
 *
 * @name webview.destroy
 *
 * @param webview_id [type:number] The webview id (returned by the webview.create() call)
 */

/*# Open a page using an url
 *
 * Opens a web page in the webview, using an url. Once the request is done, the
 * callback (registered in `webview.create()`) is invoked.
 *
 * @name webview.open
 *
 * @param webview_id [type:number] The webview id
 * @param url [type:string] The url to open
 * @param options [type:table] A table of options for the request. Currently it holds these options
 *
 * `hidden`
 * : [type:boolean] If true, the webview will stay hidden (default=false)
 *
 * @return id [type:number] The id number of the request
 *
 * @examples
 *
 * ```lua
 * local request_id = webview.open(webview_id, "http://www.defold.com", {hidden = true})
 * ```
 */


/*# Open a page using html
 *
 * Opens a web page in the webview, using html data. Once the request is done,
 * the callback (registered in `webview.create()`) is invoked.
 *
 * @name webview.open_raw
 *
 * @param webview_id [type:number] The webview id
 * @param html [type:string] The html data to display
 * @param options [type:table] A table of options for the request. See webview.open()
 *
 * @return id [type:number] The id number of the request
 *
 * @examples
 *
 * ```lua
 * local html = sys.load_resource("/main/data/test.html")
 * local request_id = webview.open_raw(webview_id, html, {hidden = true})
 * ```
 */


/*# Evaluates javascript in a webview
 *
 * Evaluates java script within the context of the currently loaded page (if any).
 * Once the request is done, the callback (registered in `webview.create()`)
 * is invoked. The callback will get the result in the `data["result"]` field.
 *
 * @name webview.eval
 *
 * @param webview_id [type:number] The webview id
 * @param code [type:string] The java script code to evaluate
 * @return id [type:number] The id number of the request
 *
 * @examples
 *
 * ```lua
 * local request_id = webview.eval(webview_id, "GetMyFormData()")
 * ```
 */

/*# Shows or hides a web view
 *
 * Shows or hides a web view
 *
 * @name webview.set_visible
 *
 * @param webview_id [type:number] The webview id (returned by the `webview.create()` call)
 * @param visible [type:number] If 0, hides the webview. If non zero, shows the view
 */


/*# Gets the visibility state of the webview
 *
 * Returns the visibility state of the webview.
 *
 * @name webview.is_visible
 *
 * @param webview_id [type:number] The webview id (returned by the webview.create() call)
 * @return visibility [type:number] Returns 0 if not visible, 1 if it is visible
 */










