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

(ns editor.lua
  (:require [clojure.edn :as edn]
            [clojure.java.io :as io]
            [clojure.set :as set]
            [clojure.spec.alpha :as s]
            [clojure.string :as string]
            [editor.code-completion :as code-completion]
            [editor.editor-extensions.ui-docs :as ui-docs]
            [editor.lua-completion :as lua-completion]
            [editor.protobuf :as protobuf]
            [editor.system :as system]
            [internal.util :as util]
            [util.coll :refer [pair]])
  (:import [com.dynamo.scriptdoc.proto ScriptDoc$Document]
           [java.net URI]
           [org.apache.commons.io FilenameUtils]))

(set! *warn-on-reflection* true)

(def ^:private docs
  ["base" "bit" "buffer" "b2d" "b2d.body" "builtins" "camera" "collectionfactory"
   "collectionproxy" "coroutine" "crash" "debug" "factory" "go" "gui" "graphics"
   "html5" "http" "image" "io" "json" "label" "liveupdate" "math" "model" "msg"
   "os" "package" "particlefx" "physics" "profiler" "render" "resource"
   "socket" "sound" "sprite" "string" "sys" "table" "tilemap" "timer" "vmath"
   "window" "zlib"])

(defn- sdoc-path [doc]
  (format "doc/%s_doc.sdoc" doc))

(defn- load-sdoc [doc-name]
  (:elements (protobuf/read-map-with-defaults ScriptDoc$Document (io/resource (sdoc-path doc-name)))))

(defn make-completion-map
  "Make a completion map from reducible of ns path + completion tuples

  Args:
    ns-path       a vector of strings that defines a nested context for the
                  completion, e.g. [\"socket\" \"dns\"] with the completion
                  \"toip\" defines a completion socket.dns.toip
    completion    a completion map

  Returns a map from completion context string (e.g. \"socket.dns\") to
  available completions for that context."
  [ns-path+completions]
  (letfn [(ensure-modules [acc ns-path]
            (reduce (fn [acc module-path]
                      (let [parent-path (pop module-path)
                            module-name (peek module-path)
                            k [:module module-name]]
                        (update acc (string/join "." parent-path)
                                (fn [m]
                                  (if (contains? m k)
                                    m
                                    (assoc m k (code-completion/make module-name :type :module)))))))
                    acc
                    (drop 1 (reductions conj [] ns-path))))]
    (let [context->key->completion
          (reduce
            (fn [acc [ns-path completion]]
              (-> acc
                  (ensure-modules ns-path)
                  (update (string/join "." ns-path) assoc [(:type completion) (:display-string completion)] completion)))
            {}
            ns-path+completions)]
      (into {}
            (map (fn [[context key->completion]]
                   [context (vec (sort-by :display-string (vals key->completion)))]))
            context->key->completion))))

(s/def ::documentation
  (s/map-of string? (s/coll-of ::code-completion/completion)))

(defn- defold-documentation []
  {:post [(s/assert ::documentation %)]}
  (make-completion-map
    (eduction
      (mapcat (fn [doc-name]
                (eduction
                  (map #(pair doc-name %))
                  (load-sdoc doc-name))))
      (map (fn [[doc-name raw-element]]
             (let [raw-name (:name raw-element)
                   name-parts (string/split raw-name #"\.")
                   ns-path (pop name-parts)
                   {:keys [type parameters] :as el} (assoc raw-element :name (peek name-parts))
                   base-url (URI. (str "https://defold.com/ref/" doc-name))
                   site-url (str "#"
                                 (string/replace
                                   (str
                                     raw-name
                                     (when (= :function type)
                                       (str ":" (string/join "-" (mapv :name parameters)))))
                                   #"[\"#*]" ""))]
               [ns-path (lua-completion/make el :base-url base-url :url site-url)])))
      docs)))

(def logic-keywords #{"and" "or" "not"})

(def self-keyword #{"self"})

(def control-flow-keywords #{"break" "do" "else" "for" "if" "elseif" "return" "then" "repeat" "while" "until" "end" "function"
                             "local" "goto" "in"})

(def defold-keywords #{"final" "init" "on_input" "on_message" "on_reload" "update" "acquire_input_focus" "disable" "enable"
                       "release_input_focus" "request_transform" "set_parent" "transform_response"})

(def lua-constants #{"nil" "false" "true"})

(def all-keywords
  (set/union logic-keywords
             self-keyword
             control-flow-keywords
             defold-keywords))

(def defold-docs (defold-documentation))
(def preinstalled-modules (into #{} (remove #{""} (keys defold-docs))))

(def base-globals
  (into #{"coroutine" "package" "string" "table" "math" "io" "file" "os" "debug"}
        (map :name)
        (load-sdoc "base")))

(defn extract-globals-from-completions [completions]
  (into #{}
        (comp
          (remove #(#{:message :property} (:type %)))
          (map :name)
          (remove #(or (= "" %)
                       (string/includes? % ":")
                       (contains? base-globals %))))
        (get completions "")))

(def defined-globals
  (extract-globals-from-completions defold-docs))

(defn- lua-base-documentation []
  {:post [(s/assert ::documentation %)]}
  {"" (into []
            (util/distinct-by :display-string)
            (concat (map #(assoc % :type :snippet)
                         (-> (io/resource "lua-base-snippets.edn")
                             slurp
                             edn/read-string))
                    (map #(code-completion/make % :type :constant)
                         lua-constants)
                    (map #(code-completion/make % :type :keyword)
                         all-keywords)))})

(def std-libs-docs (lua-base-documentation))

(defn lua-module->path [module]
  (str "/" (string/replace module #"\." "/") ".lua"))

(defn path->lua-module [path]
  (-> (if (string/starts-with? path "/") (subs path 1) path)
      (string/replace #"/" ".")
      (FilenameUtils/removeExtension)))

(defn lua-module->build-path [module]
  (str (lua-module->path module) "c"))

(defn- make-completion-info-completions [vars functions module-alias]
  (eduction
    cat
    [(eduction
       (map (fn [var-name]
              [[] (code-completion/make var-name :type :variable)]))
       vars)
     (eduction
       (map (fn [[qualified-name {:keys [params]}]]
              (let [name-parts (string/split qualified-name #"\.")
                    ns-path (pop name-parts)
                    ns-path (if (and module-alias (pos? (count ns-path)))
                              (assoc ns-path 0 module-alias)
                              ns-path)
                    name (peek name-parts)]
                [ns-path (code-completion/make
                           name
                           :type :function
                           :display-string (str name "(" (string/join ", " params) ")")
                           :insert (str name
                                        "("
                                        (->> params
                                             (map-indexed #(format "${%s:%s}" (inc %1) %2))
                                             (string/join ", "))
                                        ")"))])))
       functions)]))

(defn- make-ast-completions [local-completion-info required-completion-infos]
  (make-completion-map
    (eduction
      cat
      [(make-completion-info-completions
         (set/union (:vars local-completion-info) (:local-vars local-completion-info))
         (merge (:functions local-completion-info) (:local-functions local-completion-info))
         nil)
       (let [module->completion-info (into {} (map (juxt :module identity)) required-completion-infos)]
         (eduction
           (mapcat (fn [[alias module]]
                     (let [completion-info (module->completion-info module)]
                       (make-completion-info-completions (:vars completion-info)
                                                         (:functions completion-info)
                                                         alias))))
           (:requires local-completion-info)))])))

(defn combine-completions
  [local-completion-info required-completion-infos script-intelligence-completions]
  (merge-with (fn [dest new]
                (let [taken-display-strings (into #{} (map :display-string) dest)]
                  (into dest
                        (comp (remove #(contains? taken-display-strings (:display-string %)))
                              (util/distinct-by :display-string))
                        new)))
              defold-docs
              std-libs-docs
              script-intelligence-completions
              (make-ast-completions local-completion-info required-completion-infos)))

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
        {:name "editor.resource_exists"
         :type :function
         :description "Check if a given resource path exists in the project"
         :parameters [{:name "resource_path"
                       :types ["string"]
                       :doc "Resource path (starting with <code>/</code>) of a directory to look up"}]
         :returnvalues [{:name "value"
                         :types ["boolean"]
                         :doc ""}]}
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
                         :doc "A table with following keys:<dl>
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
         :description (format "A string, version name of Defold.\n\ne.g. `\"%s\"`" (system/defold-version))}
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
                                      (string/replace (name enum-id) \_ \space)
                                      " enums")}]
                  (eduction
                    (map (fn [enum-value]
                           {:name (str "editor.ui." id "." (ui-docs/->screaming-snake-case enum-value))
                            :type :constant
                            :description (format "`\"%s\"`" (name enum-value))}))
                    enum-values)]))))
         ui-docs/enums)
       (ui-docs/script-docs)])))

(def editor-completions
  (make-completion-map
    (eduction
      (map (fn [doc]
             (let [name-parts (string/split (:name doc) #"\.")]
               [(pop name-parts) (lua-completion/make (assoc doc :name (peek name-parts)))])))
      (editor-script-docs))))
