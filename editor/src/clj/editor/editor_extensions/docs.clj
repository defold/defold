;; Copyright 2020-2025 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.editor-extensions.docs
  (:require [clojure.string :as string]
            [editor.editor-extensions.prefs-docs :as prefs-docs]
            [editor.editor-extensions.ui-docs :as ui-docs]
            [editor.lua-completion :as lua-completion]
            [util.eduction :as e]))

(defn editor-script-docs
  "Returns a vector with script doc maps covering the editor API"
  []
  (let [node-param {:name "node"
                    :types ["string" "userdata"]
                    :doc "Either resource path (e.g. <code>\"/main/game.script\"</code>), or internal node id passed to the script by the editor"}
        property-param {:name "property"
                        :types ["string"]
                        :doc "Either <code>\"path\"</code>, <code>\"text\"</code>, or a property from the Outline view (hover the label to see its editor script name)"}
        http-status-param {:name "[status]"
                           :types ["integer"]
                           :doc "HTTP status code, an integer, default 200"}
        http-headers-param {:name "[headers]"
                            :types ["table&lt;string,string&gt;"]
                            :doc "HTTP response headers, a table from lower-case header names to header values"}
        http-response-param {:name "response" :types ["response"] :doc "HTTP response value, userdata"}
        resource-path-param {:name "resource_path"
                             :types ["string"]
                             :doc "Resource path (starting with <code>/</code>)"}
        transaction-step-param {:name "tx"
                                :types ["transaction_step"]
                                :doc "A transaction step"}
        boolean-ret-param {:name "value" :types ["boolean"] :doc ""}]
    (vec
      (e/concat
        [{:name "editor"
          :type :module
          :description "The editor module, only available when the Lua code is run as [editor script](https://defold.com/manuals/editor-scripts/)."}
         {:name "editor.get"
          :type :function
          :parameters [node-param property-param]
          :returnvalues [{:name "value"
                          :types ["any"]
                          :doc "property value"}]
          :description "Get a value of a node property inside the editor.\n\nSome properties might be read-only, and some might be unavailable in different contexts, so you should use `editor.can_get()` before reading them and `editor.can_set()` before making the editor set them."}
         {:name "editor.can_get"
          :type :function
          :parameters [node-param property-param]
          :returnvalues [boolean-ret-param]
          :description "Check if you can get this property so `editor.get()` won't throw an error"}
         {:name "editor.can_add"
          :type :function
          :parameters [node-param property-param]
          :returnvalues [boolean-ret-param]
          :description "Check if `editor.tx.add()` (as well as `editor.tx.clear()` and `editor.tx.remove()`) transaction with this property won't throw an error"}
         {:name "editor.can_set"
          :type :function
          :parameters [node-param property-param]
          :returnvalues [boolean-ret-param]
          :description "Check if `editor.tx.set()` transaction with this property won't throw an error"}
         {:name "editor.can_reorder"
          :type :function
          :parameters [node-param property-param]
          :returnvalues [boolean-ret-param]
          :description "Check if `editor.tx.reorder()` transaction with this property won't throw an error"}
         {:name "editor.can_reset"
          :type :function
          :parameters [node-param property-param]
          :returnvalues [boolean-ret-param]
          :description "Check if `editor.tx.reset()` transaction with this property won't throw an error"}
         {:name "editor.command"
          :type :function
          :description "Create an editor command"
          :parameters [{:name "opts"
                        :types ["table"]
                        :doc (str "A table with the following keys:"
                                  (lua-completion/args-doc-html
                                    [{:name "label"
                                      :types ["string" "message"]
                                      :doc "required, user-visible command name, either a string or a localization message"}
                                     {:name "locations"
                                      :types ["string[]"]
                                      :doc "required, a non-empty list of locations where the command is displayed in the editor, values are either <code>\"Edit\"</code>, <code>\"View\"</code>, <code>\"Project\"</code>, <code>\"Debug\"</code> (the editor menubar), <code>\"Assets\"</code> (the assets pane), or <code>\"Outline\"</code> (the outline pane)"}
                                     {:name "query"
                                      :types ["table"]
                                      :doc (str "optional, a query that both controls the command availability and provides additional information to the command handler functions; a table with the following keys:"
                                                (lua-completion/args-doc-html
                                                  [{:name "selection"
                                                    :types ["table"]
                                                    :doc (str "current selection, a table with the following keys:"
                                                              (lua-completion/args-doc-html
                                                                [{:name "type"
                                                                  :types ["string"]
                                                                  :doc "either <code>\"resource\"</code> (selected resource) or <code>\"outline\"</code> (selected outline node)"}
                                                                 {:name "cardinality"
                                                                  :types ["string"]
                                                                  :doc "either <code>\"one\"</code> (will use first selected item) or <code>\"many\"</code> (will use all selected items)"}]))}
                                                   {:name "argument"
                                                    :types ["table"]
                                                    :doc "the command argument"}]))}
                                     {:name "id"
                                      :types ["string"]
                                      :doc "optional, keyword identifier that may be used for assigning a shortcut to a command; should be a dot-separated identifier string, e.g. <code>\"my-extension.do-stuff\"</code>"}
                                     {:name "active"
                                      :types ["function"]
                                      :doc "optional function that additionally checks if a command is active in the current context; will receive opts table with values populated by the query; should be fast to execute since the editor might invoke it in response to UI interactions (on key typed, mouse clicked)"}
                                     {:name "run"
                                      :types ["function"]
                                      :doc "optional function that is invoked when the user decides to execute the command; will receive opts table with values populated by the query"}]))}]
          :returnvalues [{:name "command"
                          :types ["command"]
                          :doc ""}]
          :examples "Print Git history for a file:
```
editor.command({
  label = \"Git History\",
  query = {
    selection = {
      type = \"resource\",
      cardinality = \"one\"
    }
  },
  run = function(opts)
    editor.execute(
      \"git\",
      \"log\",
      \"--follow\",
      \".\" .. editor.get(opts.selection, \"path\"),
      {reload_resources=false})
  end
})
```"}
         {:name "editor.resource_attributes"
          :type :function
          :description "Query information about a project resource"
          :parameters [resource-path-param]
          :returnvalues [{:name "value"
                          :types ["table"]
                          :doc (str "A table with the following keys:"
                                    (lua-completion/args-doc-html
                                      [{:name "exists"
                                        :types ["boolean"]
                                        :doc "whether a resource identified by the path exists in the project"}
                                       {:name "is_file"
                                        :types ["boolean"]
                                        :doc "whether the resource represents a file with some content"}
                                       {:name "is_directory"
                                        :types ["boolean"]
                                        :doc "whether the resource represents a directory"}]))}]}
         {:name "editor.create_directory"
          :type :function
          :parameters [resource-path-param]
          :description "Create a directory if it does not exist, and all non-existent parent directories.\n\nThrows an error if the directory can't be created."
          :examples "```\neditor.create_directory(\"/assets/gen\")\n```"}
         {:name "editor.create_resources"
          :type :function
          :parameters [{:name "resources"
                        :types ["string[]"]
                        :doc (str "Array of resource paths (strings starting with <code>/</code>) or resource definitions, lua tables with the following keys:"
                                  (lua-completion/args-doc-html
                                    [{:name "1"
                                      :types ["string"]
                                      :doc "required, resource path (starting with <code>/</code>)"}
                                     {:name "2"
                                      :types ["string"]
                                      :doc "optional, created resource content"}]))}]
          :description "Create resources (including non-existent parent directories).\n\nThrows an error if any of the provided resource paths already exist"
          :examples "Create a single resource from template:
```
editor.create_resources({
  \"/npc.go\"
})
```
Create multiple resources:
```
editor.create_resources({
  \"/npc.go\",
  \"/levels/1.collection\",
  \"/levels/2.collection\",
})
```
Create a resource with custom content:
```
editor.create_resources({
  {\"/npc.script\", \"go.property('hp', 100)\"}
})
```"}
         {:name "editor.delete_directory"
          :type :function
          :parameters [resource-path-param]
          :description "Delete a directory if it exists, and all existent child directories and files.\n\nThrows an error if the directory can't be deleted."
          :examples "```\neditor.delete_directory(\"/assets/gen\")\n```"}
         {:name "editor.external_file_attributes"
          :type :function
          :description "Query information about file system path"
          :parameters [{:name "path"
                        :types ["string"]
                        :doc "External file path, resolved against project root if relative"}]
          :returnvalues [{:name "attributes"
                          :types ["table"]
                          :doc "A table with the following keys:<dl>
                                                 <dt><code>path <small>string</small></code></dt>
                                                 <dd>resolved file path</dd>
                                                 <dt><code>exists <small>boolean</small></code></dt>
                                                 <dd>whether there is a file system entry at the path</dd>
                                                 <dt><code>is_file <small>boolean</small></code></dt>
                                                 <dd>whether the path corresponds to a file</dd>
                                                 <dt><code>is_directory <small>boolean</small></code></dt>
                                                 <dd>whether the path corresponds to a directory</dd>
                                               </dl>"}]}
         {:name "editor.execute"
          :type :function
          :parameters [{:name "command"
                        :types ["string"]
                        :doc "Shell command name to execute"}
                       {:name "[...]"
                        :types ["string"]
                        :doc "Optional shell command arguments"}
                       {:name "[options]"
                        :types ["table"]
                        :doc "Optional options table. Supported entries:
                                           <ul>
                                             <li>
                                               <span class=\"type\">boolean</span> <code>reload_resources</code>: make the editor reload the resources from disk after the command is executed, default <code>true</code>
                                             </li>
                                             <li>
                                               <span class=\"type\">string</span> <code>out</code>: standard output mode, either:
                                               <ul>
                                                 <li>
                                                   <code>\"pipe\"</code>: the output is piped to the editor console (this is the default behavior).
                                                 </li>
                                                 <li>
                                                   <code>\"capture\"</code>: capture and return the output to the editor script with trailing newlines trimmed.
                                                 </li>
                                                 <li>
                                                   <code>\"discard\"</code>: the output is discarded completely.
                                                 </li>
                                               </ul>
                                             </li>
                                             <li>
                                               <span class=\"type\">string</span> <code>err</code>: standard error output mode, either:
                                               <ul>
                                                 <li>
                                                   <code>\"pipe\"</code>: the error output is piped to the editor console (this is the default behavior).
                                                 </li>
                                                 <li>
                                                   <code>\"stdout\"</code>: the error output is redirected to the standard output of the process.
                                                 </li>
                                                 <li>
                                                   <code>\"discard\"</code>: the error output is discarded completely.
                                                 </li>
                                               </ul>
                                             </li>
                                           </ul>"}]
          :returnvalues [{:name "result"
                          :types ["nil" "string"]
                          :doc "If <code>out</code> option is set to <code>\"capture\"</code>, returns the output as string with trimmed trailing newlines. Otherwise, returns <code>nil</code>."}]
          :description "Execute a shell command.\n\nAny shell command arguments should be provided as separate argument strings to this function. If the exit code of the process is not zero, this function throws error. By default, the function returns `nil`, but it can be configured to capture the output of the shell command as string and return it — set `out` option to `\"capture\"` to do it.<br>By default, after this shell command is executed, the editor will reload resources from disk."
          :examples "Make a directory with spaces in it:\n```\neditor.execute(\"mkdir\", \"new dir\")\n```\nRead the git status:\n```\nlocal status = editor.execute(\"git\", \"status\", \"--porcelain\", {\n  reload_resources = false,\n  out = \"capture\"\n})\n```"}
         {:name "editor.bob"
          :type :function
          :parameters [{:name "[options]"
                        :types ["table"]
                        :doc "table of command line options for bob, without the leading dashes (<code>--</code>). You can use snake_case instead of kebab-case for option keys. Only long option names are supported (i.e. <code>output</code>, not <code>o</code>). Supported value types are strings, integers and booleans. If an option takes no arguments, use a boolean (i.e. <code>true</code>). If an option may be repeated, you can use an array of values."}
                       {:name "[...commands]"
                        :types ["string"]
                        :doc "bob commands, e.g. <code>\"resolve\"</code> or <code>\"build\"</code>"}]
          :returnvalues []
          :description "Run bob the builder program\n\nFor the full documentation of the available commands and options, see [the bob manual](https://defold.com/manuals/bob/)."
          :examples "Print help in the console:\n```\neditor.bob({help = true})\n```\n\nBundle the game for the host platform:\n```\nlocal opts = {\n    archive = true,\n    platform = editor.platform\n}\neditor.bob(opts, \"distclean\", \"resolve\", \"build\", \"bundle\")\n```\nUsing snake_cased and repeated options:\n```\nlocal opts = {\n    archive = true,\n    platform = editor.platform,\n    build_server = \"https://build.my-company.com\",\n    settings = {\"test.ini\", \"headless.ini\"}\n}\neditor.bob(opts, \"distclean\", \"resolve\", \"build\")\n```\n"}
         {:name "editor.browse"
          :type :function
          :parameters [{:name "url"
                        :types ["string"]
                        :doc "http(s) or file URL"}]
          :description "Open a URL in the default browser or a registered application"}
         {:name "editor.open_external_file"
          :type :function
          :parameters [{:name "path"
                        :types ["string"]
                        :doc "file path"}]
          :description "Open a file in a registered application"}
         {:name "editor.platform"
          :type :variable
          :description "Editor platform id.\n\nA `string`, either:\n- `\"x86_64-win32\"`\n- `\"x86_64-macos\"`\n- `\"arm64-macos\"`\n- `\"x86_64-linux\"`"}
         {:name "editor.save"
          :type :function
          :parameters []
          :description "Persist any unsaved changes to disk"}
         {:name "editor.transact"
          :type :function
          :parameters [{:name "txs"
                        :types ["transaction_step[]"]
                        :doc "An array of transaction steps created using <code>editor.tx.*</code> functions"}]
          :description "Change the editor state in a single, undoable transaction"}
         {:name "editor.tx"
          :type :module
          :description "The editor module that defines function for creating transaction steps for `editor.transact()` function."}
         {:name "editor.tx.set"
          :type :function
          :parameters [node-param
                       property-param
                       {:name "value"
                        :types ["any"]
                        :doc "A new value for the property"}]
          :returnvalues [transaction-step-param]
          :description "Create transaction step that will set the node's property to a supplied value when transacted with `editor.transact()`."}
         {:name "editor.tx.add"
          :type :function
          :parameters [node-param
                       property-param
                       {:name "value"
                        :types ["any"]
                        :doc "Added item for the property, a table from property key to either a valid <code>editor.tx.set()</code>-able value, or an array of valid <code>editor.tx.add()</code>-able values"}]
          :description "Create a transaction step that will add a child item to a node's list property when transacted with `editor.transact()`."}
         {:name "editor.tx.clear"
          :type :function
          :parameters [node-param property-param]
          :returnvalues [transaction-step-param]
          :description "Create a transaction step that will remove all items from node's list property when transacted with `editor.transact()`."}
         {:name "editor.tx.remove"
          :type :function
          :parameters [node-param property-param (assoc node-param :name "child_node")]
          :returnvalues [transaction-step-param]
          :description "Create a transaction step that will remove a child node from the node's list property when transacted with `editor.transact()`."}
         {:name "editor.tx.reorder"
          :type :function
          :parameters [node-param property-param {:name "child_nodes" :types ["table"] :doc "array of child nodes (the same as returned by <code>editor.get(node, property)</code>) in new order"}]
          :returnvalues [transaction-step-param]
          :description "Create a transaction step that reorders child nodes in a node list defined by the property if supported (see <code>editor.can_reorder()</code>)"}
         {:name "editor.tx.reset"
          :type :function
          :parameters [node-param property-param]
          :returnvalues [transaction-step-param]
          :description "Create a transaction step that will reset an overridden property to its default value when transacted with `editor.transact()`."}
         {:name "editor.version"
          :type :variable
          :description "A string, version name of Defold"}
         {:name "editor.engine_sha1"
          :type :variable
          :description "A string, SHA1 of Defold engine"}
         {:name "editor.editor_sha1"
          :type :variable
          :description "A string, SHA1 of Defold editor"}
         {:name "editor.ui"
          :type :module
          :description "Module with functions for creating UI elements in the editor"}
         {:name "editor.ui.open_resource"
          :type :function
          :description "Open a resource, either in the editor or in a third-party app"
          :parameters [resource-path-param]}]
        (e/mapcat
          (fn [[enum-id enum-values]]
            (let [id (ui-docs/->screaming-snake-case enum-id)]
              (eduction
                cat
                [[{:name (str "editor.ui." id)
                   :type :module
                   :description (str "Constants for "
                                     (string/replace (name enum-id) \- \space)
                                     " enums")}]
                 (eduction
                   (map (fn [enum-value]
                          {:name (str "editor.ui." id "." (ui-docs/->screaming-snake-case enum-value))
                           :type :constant
                           :description (format "`\"%s\"`" (name enum-value))}))
                   enum-values)])))
          ui-docs/enums)
        (ui-docs/script-docs)
        (prefs-docs/script-docs)
        [{:name "http"
          :type :module
          :description "HTTP client and editor's HTTP server-related functionality"}
         {:name "http.request"
          :type :function
          :description "Perform an HTTP request"
          :parameters [{:name "url"
                        :types ["string"]
                        :doc "request URL"}
                       {:name "[opts]"
                        :types ["table"]
                        :doc (str "Additional request options, a table with the following keys:"
                                  (lua-completion/args-doc-html
                                    [{:name "method"
                                      :types ["string"]
                                      :doc "request method, defaults to <code>\"GET\"</code>"}
                                     {:name "headers"
                                      :types ["table"]
                                      :doc "request headers, a table with string keys and values"}
                                     {:name "body"
                                      :types ["string"]
                                      :doc "request body"}
                                     {:name "as"
                                      :types ["string"]
                                      :doc "response body converter, either <code>\"string\"</code> or <code>\"json\"</code>"}]))}]
          :returnvalues [{:name "response"
                          :types ["table"]
                          :doc (str "HTTP response, a table with the following keys:"
                                    (lua-completion/args-doc-html
                                      [{:name "status"
                                        :types ["integer"]
                                        :doc "response code"}
                                       {:name "headers"
                                        :types ["table"]
                                        :doc "response headers, a table where each key is a lower-cased string, and each value is either a string or an array of strings if the header was repeated"}
                                       {:name "body"
                                        :types ["string" "any" "nil"]
                                        :doc "response body, present only when <code>as</code> option was provided, either a string or a parsed json value"}]))}]}
         {:name "http.server"
          :type :module
          :description "Editor's HTTP server-related functionality"}
         {:name "http.server.local_url"
          :type :constant
          :description "Editor's HTTP server local url"}
         {:name "http.server.port"
          :type :constant
          :description "Editor's HTTP server port"}
         {:name "http.server.url"
          :type :constant
          :description "Editor's HTTP server url"}
         {:name "http.server.external_file_response"
          :type :function
          :description "Create HTTP response that will stream the content of a file defined by the path"
          :parameters [{:name "path"
                        :types ["string"]
                        :doc "External file path, resolved against project root if relative"}
                       http-status-param
                       http-headers-param]
          :returnvalues [http-response-param]}
         {:name "http.server.json_response"
          :type :function
          :description "Create HTTP response with a JSON value"
          :parameters [{:name "value"
                        :types ["any"]
                        :doc "Any Lua value that may be represented as JSON"}
                       http-status-param
                       http-headers-param]
          :returnvalues [http-response-param]}
         {:name "http.server.resource_response"
          :type :function
          :description "Create HTTP response that will stream the content of a resource defined by the resource path"
          :parameters [resource-path-param
                       http-status-param
                       http-headers-param]
          :returnvalues [http-response-param]}
         {:name "http.server.response"
          :type :function
          :description "Create HTTP response"
          :parameters [http-status-param
                       http-headers-param
                       {:name "[body]"
                        :types ["string"]
                        :doc "HTTP response body"}]
          :returnvalues [http-response-param]}
         {:name "http.server.route"
          :type :function
          :description "Create route definition for the editor's HTTP server"
          :parameters [{:name "path"
                        :types ["string"]
                        :doc "HTTP URI path, starts with <code>/</code>; may include path patterns (<code>{name}</code> for a single segment and <code>{*name}</code> for the rest of the request path) that will be extracted from the path and provided to the handler as a part of the request"}
                       {:name "[method]"
                        :types ["string"]
                        :doc "HTTP request method, default <code>\"GET\"</code>"}
                       {:name "[as]"
                        :types ["string"]
                        :doc "Request body converter, either <code>\"string\"</code> or <code>\"json\"</code>; the body will be discarded if not specified"}
                       {:name "handler"
                        :types ["function"]
                        :doc (str "Request handler function, will receive request argument, a table with the following keys:"
                                  (lua-completion/args-doc-html
                                    [{:name "path"
                                      :types ["string"]
                                      :doc "full matched path, a string starting with <code>/</code>"}
                                     {:name "method"
                                      :types ["string"]
                                      :doc "HTTP request method, e.g. <code>\"POST\"</code>"}
                                     {:name "headers"
                                      :types ["table&lt;string,(string|string[])&gt;"]
                                      :doc "HTTP request headers, a table from lower-case header names to header values"}
                                     {:name "query"
                                      :types ["string"]
                                      :doc "optional query string"}
                                     {:name "body"
                                      :types ["string" "any"]
                                      :doc "optional request body, depends on the <code>as</code> argument"}])
                                  "\nHandler function should return either a single response value, or 0 or more arguments to the <code>http.server.response()</code> function")}]
          :returnvalues [{:name "route" :types ["route"] :doc "HTTP server route"}]
          :examples "Receive JSON and respond with JSON:
```
http.server.route(
  \"/json\", \"POST\", \"json\",
  function(request)
    pprint(request.body)
    return 200
  end
)
```
Extract parts of the path:
```
http.server.route(
  \"/users/{user}/orders\",
  function(request)
    print(request.user)
  end
)
```
Simple file server:
```
http.server.route(
  \"/files/{*file}\",
  function(request)
    local attrs = editor.external_file_attributes(request.file)
    if attrs.is_file then
      return http.server.external_file_response(request.file)
    elseif attrs.is_directory then
      return 400
    else
      return 404
    end
  end
)
```"}
         {:name "json"
          :type :module
          :description "Module for encoding or decoding values in JSON format"}
         {:name "json.decode"
          :type :function
          :description "Decode JSON string to Lua value"
          :parameters [{:name "json"
                        :types ["string"]
                        :doc "json data"}
                       {:name "[options]"
                        :types ["table"]
                        :doc (str "A table with the following keys:"
                                  (lua-completion/args-doc-html
                                    [{:name "all"
                                      :types ["boolean"]
                                      :doc "if <code>true</code>, decodes all json values in a string and returns an array"}]))}]}
         {:name "json.encode"
          :type :function
          :description "Encode Lua value to JSON string"
          :parameters [{:name "value"
                        :types ["any"]
                        :doc "any Lua value that may be represented as JSON"}]}
         {:name "pprint"
          :type :function
          :description "Pretty-print a Lua value"
          :parameters [{:name "value"
                        :types ["any"]
                        :doc "any Lua value to pretty-print"}]}]
        (let [message-pattern-ret {:name "message"
                                   :types ["message"]
                                   :doc "a userdata value that, when stringified with <code>tostring()</code>, will produce a localized text according to the currently selected language in the editor"}
              localizable-value-doc "<code>nil</code>, <code>boolean</code>, <code>number</code>, <code>string</code>, or another <code>message</code> instance"
              localizable-items-doc (str "array of values; each value may be " localizable-value-doc)]
          [{:name "localization"
            :type :module
            :description "Module for producing localizable messages for editor localization"}
           {:name "localization.message"
            :type :function
            :description "Create a message pattern for a localization key defined in an `.editor_localization` file; the actual localization happens when the returned value is stringified"
            :parameters [{:name "key"
                          :types ["string"]
                          :doc "localization key defined in an <code>.editor_localization</code> file"}
                         {:name "[vars]"
                          :types ["table"]
                          :doc (str "optional table with variables to be substituted in the localized string that uses <a href=\"https://unicode-org.github.io/icu/userguide/format_parse/messages/\">ICU Message Format</a> syntax; keys must be strings; values must be either " localizable-value-doc)}]
            :returnvalues [message-pattern-ret]}
           {:name "localization.and_list"
            :type :function
            :description "Create a message pattern that renders a list with the \"and\" conjunction (for example: a, b, and c) once it is stringified"
            :parameters [{:name "items" :types ["any[]"] :doc localizable-items-doc}]
            :returnvalues [message-pattern-ret]}
           {:name "localization.or_list"
            :type :function
            :description "Create a message pattern that renders a list with the \"or\" conjunction (for example: a, b, or c) once it is stringified"
            :parameters [{:name "items" :types ["any[]"] :doc localizable-items-doc}]
            :returnvalues [message-pattern-ret]}
           {:name "localization.concat"
            :type :function
            :description "Create a message pattern that concatenates values (similar to <code>table.concat</code>) and performs the actual concatenation when stringified"
            :parameters [{:name "items" :types ["any[]"] :doc localizable-items-doc}
                         {:name "[separator]"
                          :types ["nil" "boolean" "number" "string" "message"]
                          :doc "optional separator inserted between values; defaults to an empty string"}]
            :returnvalues [message-pattern-ret]}])
        (when (System/getProperty "defold.dev")
          ;; Dev-only docs
          [{:name "editor.bundle.abort_message"
            :type :variable
            :description "Error message the signifies bundle abort that is not an error"}
           {:name "editor.bundle.assoc"
            :type :function
            :description "Immutably set a key to value in a table"
            :parameters [{:name "table"
                          :types ["table"]
                          :doc "the table"}
                         {:name "key"
                          :types ["any"]
                          :doc "the key"}
                         {:name "value"
                          :types ["any"]
                          :doc "the value"}]
            :returnvalues [{:name "table"
                            :types ["table"]
                            :doc "New table if it should be changed by assoc, or the input table otherwise"}]}
           {:name "editor.bundle.assoc_in"
            :type :function
            :description "Immutably set a value to a nested path in a table"
            :parameters [{:name "table"
                          :types ["table"]
                          :doc "the table"}
                         {:name "keys"
                          :types ["any[]"]
                          :doc "the keys"}
                         {:name "value"
                          :types ["any"]
                          :doc "the value"}]
            :returnvalues [{:name "table"
                            :types ["table"]
                            :doc "New table if it should be changed by assoc_in, or the input table otherwise"}]}
           {:name "editor.bundle.make_to_string_lookup"
            :type :function
            :description "Make stringifier function that first performs the label lookup in a provided table"
            :parameters [{:name "table"
                          :types ["table"]
                          :doc "table from values to their corresponding string representation"}]
            :returnvalues [{:name "to_string"
                            :types ["function"]
                            :doc "stringifier function"}]}
           {:name "editor.bundle.select_box"
            :type :function
            :description "Helper function for creating a select box component"
            :parameters [{:name "config" :types ["table"] :doc "config table"}
                         {:name "set_config" :types ["function"] :doc "config setter"}
                         {:name "key" :types ["string"] :doc "config key for the selected value"}
                         {:name "options" :types ["any[]"] :doc "select box options"}
                         {:name "to_string" :types ["function"] :doc "option stringifier"}
                         {:name "[rest_props]" :types ["table"] :doc "extra props for <code>editor.ui.select_box</code>"}]
            :returnvalues [{:name "select_box" :types ["component"] :doc "UI component"}]}
           {:name "editor.bundle.check_box"
            :type :function
            :description "Helper function for creating a check box component"
            :parameters [{:name "config" :types ["table"] :doc "config table"}
                         {:name "set_config" :types ["function"] :doc "config setter"}
                         {:name "key" :types ["string"] :doc "config key for the selected value"}
                         {:name "text" :types ["string"] :doc "check box label text"}
                         {:name "[rest_props]" :types ["table"] :doc "extra props for <code>editor.ui.check_box</code>"}]
            :returnvalues [{:name "check_box" :types ["component"] :doc "UI component"}]}
           {:name "editor.bundle.set_element_check_box"
            :type :function
            :description "Helper function for creating a check box for an enum value of set config key"
            :parameters [{:name "config" :types ["table"] :doc "config map with common boolean keys"}
                         {:name "set_config" :types ["function"] :doc "config setter"}
                         {:name "key" :types ["string"] :doc "config key for the set"}
                         {:name "element" :types ["string"] :doc "enum value in the set"}
                         {:name "text" :types ["string"] :doc "check box label text"}
                         {:name "[error]" :types ["string"] :doc "error message"}]
            :returnvalues [{:name "check_box" :types ["component"] :doc "UI component"}]}
           {:name "editor.bundle.external_file_field"
            :type :function
            :description "Helper function for creating an external file field component"
            :parameters [{:name "config" :types ["table"] :doc "config map with common boolean keys"}
                         {:name "set_config" :types ["function"] :doc "config setter"}
                         {:name "key" :types ["string"] :doc "config key for the set"}
                         {:name "[error]" :types ["string"] :doc "error message"}
                         {:name "[rest_props]" :types ["table"] :doc "extra props for <code>editor.ui.external_file_field</code>"}]
            :returnvalues [{:name "external_file_field" :types ["component"] :doc "UI component"}]}
           {:name "editor.bundle.dialog"
            :type :function
            :description "Helper function for creating a bundle dialog component"
            :parameters [{:name "heading" :types ["string"] :doc "dialog heading text"}
                         {:name "config" :types ["table"] :doc "config map with common boolean keys"}
                         {:name "hint" :types ["string" "nil"] :doc "dialog hint text"}
                         {:name "error" :types ["string" "nil"] :doc "dialog error text"}
                         {:name "rows" :types ["component[][]"] :doc "grid rows of UI elements, dialog content"}]
            :returnvalues [{:name "dialog" :types ["component"] :doc "UI component"}]}
           {:name "editor.bundle.grid_row"
            :type :function
            :description "Return a 2-element array that represents a single grid row in a bundle dialog"
            :parameters [{:name "text"
                          :types ["string" "nil"]
                          :doc "optional string label"}
                         {:name "content"
                          :types ["component" "component[]"]
                          :doc "either a single UI component, or an array of components (will be laid out vertically)"}]
            :returnvalues [{:name "row"
                            :types ["component[]"]
                            :doc "a single grid row"}]}
           {:name "editor.bundle.desktop_variant_grid_row"
            :type :function
            :description "Create a grid row for the desktop variant setting"
            :parameters [{:name "config" :types ["table"] :doc "config table with <code>variant</code> key"}
                         {:name "set_config" :types ["function"] :doc "config setter"}]
            :returnvalues [{:name "row" :types ["component[]"] :doc "grid row"}]}
           {:name "editor.bundle.common_variant_grid_row"
            :type :function
            :description "Create a grid row for the common variant setting"
            :parameters [{:name "config" :types ["table"] :doc "config map with <code>variant</code> key"}
                         {:name "set_config" :types ["function"] :doc "config setter"}]
            :returnvalues [{:name "row" :types ["component[]"] :doc "grid row"}]}
           {:name "editor.bundle.texture_compression_grid_row"
            :type :function
            :description "Create a grid row for the texture compression setting"
            :parameters [{:name "config" :types ["table"] :doc "config map with <code>texture_compression</code> key"}
                         {:name "set_config" :types ["function"] :doc "config setter"}]
            :returnvalues [{:name "row" :types ["component[]"] :doc "grid row"}]}
           {:name "editor.bundle.check_boxes_grid_row"
            :type :function
            :description "Create a grid row for the common boolean settings"
            :parameters [{:name "config" :types ["table"] :doc "config map with common boolean keys"}
                         {:name "set_config" :types ["function"] :doc "config setter"}]
            :returnvalues [{:name "row" :types ["component[]"] :doc "grid row"}]}
           {:name "editor.bundle.config"
            :type :function
            :description "Get bundle config, optionally showing a dialog to edit the config"
            :parameters [{:name "requested_dialog"
                          :types ["boolean"]
                          :doc "whether the user explicitly requested a dialog"}
                         {:name "prefs_key"
                          :types ["string"]
                          :doc "preference key used for loading the bundle config"}
                         {:name "dialog_component"
                          :types ["component"]
                          :doc "UI component for the dialog, will receive <code>config</code> and (optional) <code>errors</code> props"}
                         {:name "[errors_fn]"
                          :types ["function"]
                          :doc "error checking predicate, takes config as an argument; if returns truthy value, it will be passed as a prop to <code>dialog_component</code>"}]
            :returnvalues [{:name "config"
                            :types ["any"]}]}
           {:name "editor.bundle.output_directory"
            :type :function
            :description "Get bundle output directory, optionally showing a directory selection dialog"
            :parameters [{:name "requested_dialog"
                          :types ["boolean"]
                          :doc "whether the user explicitly requested a dialog"}
                         {:name "output_subdir"
                          :types ["string"]
                          :doc "output subdir, usually a platform name"}]}
           {:name "editor.bundle.create"
            :type :function
            :description "Create bob bundle"
            :parameters [{:name "config"
                          :types ["table"]
                          :doc "bundle config"}
                         {:name "output_directory"
                          :types ["string"]
                          :doc "bundle output directory"}
                         {:name "extra_bob_opts"
                          :types ["table"]
                          :doc "extra bob opts, presumably produced from config"}]}
           {:name "editor.bundle.command"
            :type :function
            :description "Create bundle command definition"
            :parameters [{:name "label"
                          :types ["string"]
                          :doc "Command label, as presented in the UI"}
                         {:name "id"
                          :types ["string"]
                          :doc "Command id, e.g. <code>\"bundle-my-platform\"</code>, used for re-bundling"}
                         {:name "fn"
                          :types ["function"]
                          :doc "bundle function, will receive a <code>requested_dialog</code> boolean argument"}
                         {:name "[rest]"
                          :types ["table"]
                          :doc "extra keys for the command definition, e.g. <code>active</code>"}]}
           {:name "editor.bundle.desktop_variant_schema"
            :type :variable
            :description "prefs schema for desktop bundle variants"}
           {:name "editor.bundle.common_variant_schema"
            :type :variable
            :description "prefs schema for common bundle variants"}
           {:name "editor.bundle.config_schema"
            :type :function
            :description "Helper function for constructing prefs schema for new bundle dialogs"
            :parameters [{:name "variant_schema" :types ["schema"] :doc "bundle variant schema"}
                         {:name "[properties]" :types ["table" "nil"] :doc "extra config properties"}]
            :returnvalues [{:name "schema" :types ["schema"] :doc (str "full bundle schema, defines a project-scoped object schema with the following keys:"
                                                                       (lua-completion/args-doc-html
                                                                         [{:name "variant" :doc "the provided variant schema"}
                                                                          {:name "texture_compression" :types ["string"] :doc "either <code>enabled</code>, <code>disabled</code> or <code>editor</code>"}
                                                                          {:name "with_symbols" :types ["boolean"]}
                                                                          {:name "build_report" :types ["boolean"]}
                                                                          {:name "liveupdate" :types ["boolean"]}
                                                                          {:name "contentless" :types ["boolean"]}]))}]}])
        (let [tiles-param {:name "tiles" :types ["tiles"] :doc "unbounded 2d grid of tiles"}
              x-param {:name "x" :types ["integer"] :doc "x coordinate of a tile"}
              y-param {:name "y" :types ["integer"] :doc "y coordinate of a tile"}
              tile-doc "1-indexed tile index of a tilemap's tilesource"
              info-doc (str "full tile information table with the following keys:"
                            (lua-completion/args-doc-html
                              [{:name "index" :types ["integer"] :doc tile-doc}
                               {:name "h_flip" :types ["boolean"] :doc "horizontal flip"}
                               {:name "v_flip" :types ["boolean"] :doc "vertical flip"}
                               {:name "rotate_90" :types ["boolean"] :doc "whether the tile is rotated 90 degrees clockwise"}]))
              tile-param {:name "tile_index" :types ["integer"] :doc tile-doc}
              info-param {:name "info" :types ["table"] :doc info-doc}]
          [{:name "tilemap"
            :type :module
            :description "Module for manipulating tilemaps"}
           {:name "tilemap.tiles"
            :type :module
            :description "Module for manipulating tiles on a tilemap layer"}
           {:name "tilemap.tiles.new"
            :type :function
            :description "Create a new unbounded 2d grid data structure for storing tilemap layer tiles"
            :parameters []
            :returnvalues [tiles-param]}
           {:name "tilemap.tiles.get_tile"
            :type :function
            :description "Get a tile index at a particular coordinate"
            :parameters [tiles-param x-param y-param]
            :returnvalues [tile-param]}
           {:name "tilemap.tiles.get_info"
            :type :function
            :description "Get full information from a tile at a particular coordinate"
            :parameters [tiles-param x-param y-param]
            :returnvalues [info-param]}
           {:name "tilemap.tiles.iterator"
            :type :function
            :description "Create an iterator over all tiles in a tiles data structure\n\nWhen iterating using for loop, each iteration returns x, y and tile index of a tile in a tile map"
            :parameters [tiles-param]
            :returnvalues [{:name "iter" :types ["function"] :doc "iterator"}]
            :examples "Iterate over all tiles in a tile map:
```
local layers = editor.get(\"/level.tilemap\", \"layers\")
for i = 1, #layers do
  local tiles = editor.get(layers[i], \"tiles\")
  for x, y, i in tilemap.tiles.iterator(tiles) do
    print(x, y, i)
  end
end
```"}
           {:name "tilemap.tiles.clear"
            :type :function
            :description "Remove all tiles"
            :parameters [tiles-param]
            :returnvalues [tiles-param]}
           {:name "tilemap.tiles.set"
            :type :function
            :description "Set a tile at a particular coordinate"
            :parameters [tiles-param
                         x-param
                         y-param
                         {:name "tile_or_info"
                          :types ["integer" "table"]
                          :doc (str "Either " tile-doc " or " info-doc)}]
            :returnvalues [tiles-param]}
           {:name "tilemap.tiles.remove"
            :type :function
            :description "Remove a tile at a particular coordinate"
            :parameters [tiles-param x-param y-param]
            :returnvalues [tiles-param]}])
        [{:name "zip"
          :type :module
          :description "Module for manipulating zip archives"}
         (let [method-param {:name "method" :types ["string"] :doc "compression method, either <code>zip.METHOD.DEFLATED</code> (default) or <code>zip.METHOD.STORED</code>"}
               level-param {:name "level" :types ["integer"] :doc "compression level, an integer between 0 and 9, only useful when the compression method is <code>zip.METHOD.DEFLATED</code>; defaults to 6"}]
           {:name "zip.pack"
            :type :function
            :parameters [{:name "output_path"
                          :types ["string"]
                          :doc "output zip file path, resolved against project root if relative"}
                         {:name "[opts]"
                          :types ["table"]
                          :doc (str "compression options, a table with the following keys:"
                                    (lua-completion/args-doc-html [method-param level-param]))}
                         {:name "entries"
                          :types ["string" "table"]
                          :doc (str "entries to compress, either a string (relative path to file or folder to include) or a table with the following keys:"
                                    (lua-completion/args-doc-html
                                      [{:name "1" :types ["string"] :doc "required; source file or folder path to include, resolved against project root if relative"}
                                       {:name "2" :types ["string"] :doc "optional; target file or folder path in the zip archive. May be omitted if source is a relative path that does not go above the project directory."}
                                       method-param
                                       level-param]))}]
            :description "Create a ZIP archive"
            :examples "Archive a file and a folder:
```
zip.pack(\"build.zip\", {\"build\", \"game.project\"})
```
Change the location of the files within the archive:
```
zip.pack(\"build.zip\", {
  {\"build/wasm-web\", \".\"},
  {\"configs/prod.json\", \"config.json\"}
})
```
Create archive without compression (much faster to create the archive, bigger archive file size, allows mmap access):
```
zip.pack(\"build.zip\", {method = zip.METHOD.STORED}, {
  \"build\",
  \"resources\"
})
```
Don't compress one of the folders:
```
zip.pack(\"build.zip\", {
  {\"assets\", method = zip.METHOD.STORED},
  \"build/wasm-web\"
})
```
Include files from outside the project:
```
zip.pack(\"build.zip\", {
  \"build\",
  {\"../secrets/auth-key.txt\", \"auth-key.txt\"}
})
```"})
         {:name "zip.unpack"
          :type :function
          :parameters [{:name "archive_path"
                        :types ["string"]
                        :doc "zip file path, resolved against project root if relative"}
                       {:name "[target_path]"
                        :types ["string"]
                        :doc "target path for extraction, defaults to parent of <code>archive_path</code> if omitted"}
                       {:name "[opts]"
                        :types ["table"]
                        :doc (str "extraction options, a table with the following keys:"
                                  (lua-completion/args-doc-html
                                    [{:name "on_conflict"
                                      :types ["string"]
                                      :doc "conflict resolution strategy, defaults to <code>zip.ON_CONFLICT.ERROR</code>"}]))}
                       {:name "[paths]"
                        :types ["table"]
                        :doc "entries to extract, relative string paths"}]
          :description "Extract a ZIP archive"
          :examples "Extract everything to a build dir:
```lua
zip.unpack(\"build/dev/resources.zip\")
```
Extract to a different directory:
```lua
zip.unpack(
  \"build/dev/resources.zip\",
  \"build/dev/tmp\",
)
```
Extract while overwriting existing files on conflict:
```lua
zip.unpack(
  \"build/dev/resources.zip\",
  {on_conflict = zip.ON_CONFLICT.OVERWRITE}
)
```
Extract a single file:
```lua
zip.unpack(
  \"build/dev/resources.zip\",
  {\"config.json\"}
)
```"}
         {:name "zip.METHOD"
          :type :module
          :description "Constants for zip compression methods"}
         {:name "zip.METHOD.DEFLATED"
          :type :constant
          :description "<code>\"deflated\"</code> compression method"}
         {:name "zip.METHOD.STORED"
          :type :constant
          :description "<code>\"stored\"</code> compression method, i.e. no compression"}
         {:name "zip.ON_CONFLICT"
          :type :module
          :description "Constants defining conflict resolution strategies for zip archive extraction"}
         {:name "zip.ON_CONFLICT.ERROR"
          :type :constant
          :description "`\"error\"`, any conflict aborts extraction"}
         {:name "zip.ON_CONFLICT.SKIP"
          :type :constant
          :description "`\"skip\"`, existing file is preserved"}
         {:name "zip.ON_CONFLICT.OVERWRITE"
          :type :constant
          :description "`\"skip\"`, existing file is overwritten"}]))))
