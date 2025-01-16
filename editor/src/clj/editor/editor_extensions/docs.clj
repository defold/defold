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
                        :doc "Either <code>\"path\"</code>, <code>\"text\"</code>, or a property from the Outline view (hover the label to see its editor script name)"}]
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
        [{:name "json"
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
                        :doc "any lua value that may be represented as JSON"}]}]
        (when-not (System/getProperty "defold.version")
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
                                                                          {:name "contentless" :types ["boolean"]}]))}]}])))))