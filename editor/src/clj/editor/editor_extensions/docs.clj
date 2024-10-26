;; Copyright 2020-2024 The Defold Foundation
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
            [editor.editor-extensions.ui-docs :as ui-docs]
            [editor.lua-completion :as lua-completion]))

(defn editor-script-docs
  "Returns reducible with script doc maps covering the editor API"
  []
  (let [node-param {:name "node"
                    :types ["string" "userdata"]
                    :doc "Either resource path (e.g. <code>\"/main/game.script\"</code>), or internal node id passed to the script by the editor"}
        property-param {:name "property"
                        :types ["string"]
                        :doc "Either <code>\"path\"</code>, <code>\"text\"</code>, or a property from the Outline view (hover the label to see its editor script name)"}]
    (eduction
      cat
      [[{:name "editor"
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
         :returnvalues [{:name "value"
                         :types ["boolean"]
                         :doc ""}]
         :description "Check if you can get this property so `editor.get()` won't throw an error"}
        {:name "editor.can_set"
         :type :function
         :parameters [node-param property-param]
         :returnvalues [{:name "value"
                         :types ["boolean"]
                         :doc ""}]
         :description "Check if `\"set\"` action with this property won't throw an error"}
        {:name "editor.resource_attributes"
         :type :function
         :description "Query information about a project resource"
         :parameters [{:name "resource_path"
                       :types ["string"]
                       :doc "Resource path (starting with <code>/</code>) of a resource to look up"}]
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
         :parameters [{:name "resource_path"
                       :types ["string"]
                       :doc "Resource path (starting with <code>/</code>) of a directory to create"}]
         :description "Create a directory if it does not exist, and all non-existent parent directories.\n\nThrows an error if the directory can't be created."
         :examples "```\neditor.create_directory(\"/assets/gen\")\n```"}
        {:name "editor.delete_directory"
         :type :function
         :parameters [{:name "resource_path"
                       :types ["string"]
                       :doc "Resource path (starting with <code>/</code>) of a directory to delete"}]
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
        {:name "editor.platform"
         :type :variable
         :description "Editor platform id.\n\nA `string`, either:\n- `\"x86_64-win32\"`\n- `\"x86_64-macos\"`\n- `\"arm64-macos\"`\n- `\"x86_64-linux\"` `\"arm64-linux\"`"}
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
         :returnvalues [{:name "result"
                         :types ["transaction_step"]
                         :doc "A transaction step"}]
         :description "Create a set transaction step.\n\nWhen the step is transacted using `editor.transact()`, it will set the node's property to a supplied value"}
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
         :parameters [{:name "resource_path"
                       :types ["string"]
                       :doc "Resource path (starting with <code>/</code>) of a resource to open"}]}]
       (eduction
         (mapcat
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
                    enum-values)]))))
         ui-docs/enums)
       (ui-docs/script-docs)])))