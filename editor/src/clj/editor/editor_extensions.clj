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

(ns editor.editor-extensions
  (:require [cljfx.api :as fx]
            [clojure.data.json :as json]
            [clojure.java.io :as io]
            [clojure.set :as set]
            [clojure.stacktrace :as stacktrace]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code.data :as data]
            [editor.console :as console]
            [editor.defold-project :as project]
            [editor.editor-extensions.actions :as actions]
            [editor.editor-extensions.coerce :as coerce]
            [editor.editor-extensions.commands :as commands]
            [editor.editor-extensions.error-handling :as error-handling]
            [editor.editor-extensions.graph :as graph]
            [editor.editor-extensions.prefs-docs :as prefs-docs]
            [editor.editor-extensions.prefs-functions :as prefs-functions]
            [editor.editor-extensions.runtime :as rt]
            [editor.editor-extensions.ui-components :as ui-components]
            [editor.editor-extensions.zip :as zip]
            [editor.fs :as fs]
            [editor.future :as future]
            [editor.graph-util :as gu]
            [editor.handler :as handler]
            [editor.lsp :as lsp]
            [editor.lsp.async :as lsp.async]
            [editor.os :as os]
            [editor.prefs :as prefs]
            [editor.process :as process]
            [editor.resource :as resource]
            [editor.system :as system]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [util.eduction :as e]
            [util.http-client :as http])
  (:import [com.dynamo.bob Platform]
           [com.dynamo.bob.bundle BundleHelper]
           [java.io PrintStream PushbackReader]
           [java.net URI]
           [java.nio.file FileAlreadyExistsException Files NotDirectoryException Path]
           [java.util HashSet]
           [org.luaj.vm2 LuaError LuaFunction LuaString LuaTable LuaValue Prototype]))

(set! *warn-on-reflection* true)

(defn- ext-state
  "Returns an extension state, a map with the following keys:
    :reload-resources!     0-arg function used to reload resources
    :display-output!       2-arg function used to display extension-related
                           output to the user, where args are:
                             type    output type, :err or :out
                             msg     string message, may be multiline
    :project-prototypes    vector of project-owned editor script Prototypes
    :library-prototypes    vector of library-provided editor script Prototypes
    :rt                    editor script runtime
    :all                   map of module function keyword to a vector of tuples:
                             path      proj-path of an editor script
                             lua-fn    LuaFunction identified by the keyword
    :hooks                 exists only when '/hooks.editor_script' exists, a map
                           from module function keyword to LuaFunction"
  [project evaluation-context]
  (-> project
      (g/node-value :editor-extensions evaluation-context)
      (g/user-data :state)))

(defn- execute-all-top-level-functions
  "Returns reducible that executes all specified top-level editor script fns

  Args:
    state         editor extensions state
    fn-keyword    keyword identifying the editor script function
    opts          Clojure data structure that will be coerced to Lua

  Returns a vector of path+ret tuples, removing all results that threw
  exception, where:
    path    a proj-path of the editor script, string
    ret     a Lua data structure returned from function in that file"
  [state fn-keyword opts evaluation-context]
  (let [{:keys [rt all display-output!]} state
        lua-opts (rt/->lua opts)
        label (name fn-keyword)]
    (eduction
      (keep (fn [[path lua-fn]]
              (when-let [lua-ret (error-handling/try-with-extension-exceptions
                                   :display-output! display-output!
                                   :label (str label " in " path)
                                   :catch nil
                                   (rt/invoke-immediate-1 rt lua-fn lua-opts evaluation-context))]
                (when-not (rt/coerces-to? rt coerce/null lua-ret)
                  [path lua-ret]))))
      (get all fn-keyword))))

(defn- unwrap-error-values [arr]
  (mapv #(cond-> % (g/error? %) :value) arr))

(g/defnode EditorExtensions
  (input project-prototypes g/Any :array :substitute unwrap-error-values)
  (input library-prototypes g/Any :array :substitute unwrap-error-values)
  (output project-prototypes g/Any (gu/passthrough project-prototypes))
  (output library-prototypes g/Any (gu/passthrough library-prototypes)))

(defn make [graph]
  (first (g/tx-nodes-added (g/transact (g/make-node graph EditorExtensions)))))

;; region script API

(defn- make-ext-get-fn [project]
  (rt/lua-fn ext-get [{:keys [rt evaluation-context]} lua-node-id-or-path lua-property]
    (let [node-id-or-path (rt/->clj rt graph/node-id-or-path-coercer lua-node-id-or-path)
          property (rt/->clj rt coerce/string lua-property)
          node-id (graph/node-id-or-path->node-id node-id-or-path project evaluation-context)
          getter (graph/ext-value-getter node-id property evaluation-context)]
      (if getter
        (getter)
        (throw (LuaError. (str (name (graph/node-id->type-keyword node-id evaluation-context))
                               " has no \""
                               property
                               "\" property")))))))

(defn- make-ext-can-get-fn [project]
  (rt/lua-fn ext-can-get [{:keys [rt evaluation-context]} lua-node-id-or-path lua-property]
    (let [node-id-or-path (rt/->clj rt graph/node-id-or-path-coercer lua-node-id-or-path)
          property (rt/->clj rt coerce/string lua-property)
          node-id (graph/node-id-or-path->node-id node-id-or-path project evaluation-context)]
      (some? (graph/ext-value-getter node-id property evaluation-context)))))

(defn- make-ext-can-set-fn [project]
  (rt/lua-fn ext-can-set [{:keys [rt evaluation-context]} lua-node-id-or-path lua-property]
    (let [node-id-or-path (rt/->clj rt graph/node-id-or-path-coercer lua-node-id-or-path)
          property (rt/->clj rt coerce/string lua-property)
          node-id (graph/node-id-or-path->node-id node-id-or-path project evaluation-context)]
      (some? (graph/ext-lua-value-setter node-id property rt project evaluation-context)))))

(defn- make-ext-create-directory-fn [project reload-resources!]
  (rt/suspendable-lua-fn ext-create-directory [{:keys [rt evaluation-context]} lua-proj-path]
    (let [^String proj-path (rt/->clj rt graph/resource-path-coercer lua-proj-path)]
      (let [root-path (-> project
                          (project/workspace evaluation-context)
                          (workspace/project-path evaluation-context)
                          fs/real-path)
            dir-path (-> (str root-path proj-path)
                         (fs/as-path)
                         (.normalize))]
        (if (.startsWith dir-path root-path)
          (try
            (fs/create-path-directories! dir-path)
            (future/then (reload-resources!) rt/and-refresh-context)
            (catch FileAlreadyExistsException e
              (throw (LuaError. (str "File already exists: " (.getMessage e)))))
            (catch Exception e
              (throw (LuaError. ^String (or (.getMessage e) (.getSimpleName (class e)))))))
          (throw (LuaError. (str "Can't create " dir-path ": outside of project directory"))))))))

(defn- make-ext-delete-directory-fn [project reload-resources!]
  (rt/suspendable-lua-fn ext-delete-directory [{:keys [rt evaluation-context]} lua-proj-path]
    (let [proj-path (rt/->clj rt graph/resource-path-coercer lua-proj-path)
          root-path (-> project
                        (project/workspace evaluation-context)
                        (workspace/project-path evaluation-context)
                        fs/real-path)
          dir-path (-> (str root-path proj-path)
                       (fs/as-path)
                       (.normalize))
          protected-paths (mapv #(.resolve root-path ^String %)
                                [".git"
                                 ".internal"])
          protected-path? (fn protected-path? [^Path path]
                            (some #(.startsWith path ^Path %)
                                  protected-paths))]
      (cond
        (not (.startsWith dir-path root-path))
        (throw (LuaError. (str "Can't delete " dir-path ": outside of project directory")))

        (= (.getNameCount dir-path) (.getNameCount root-path))
        (throw (LuaError. (str "Can't delete the project directory itself")))

        (protected-path? dir-path)
        (throw (LuaError. (str "Can't delete " dir-path ": protected by editor")))

        :else
        (try
          (when (fs/delete-path-directory! dir-path)
            (future/then (reload-resources!) rt/and-refresh-context))
          (catch NotDirectoryException e
            (throw (LuaError. (str "Not a directory: " (.getMessage e)))))
          (catch Exception e
            (throw (LuaError. (str (.getMessage e))))))))))

(defn- make-ext-external-file-attributes-fn [^Path project-path]
  (rt/suspendable-lua-fn ext-external-file-attributes [{:keys [rt]} lua-path]
    (let [^String path-str (rt/->clj rt coerce/string lua-path)
          path (.normalize (.resolve project-path path-str))]
      (future/io
        (if (fs/path-exists? path)
          (let [attrs (fs/path-attributes path)]
            {:path (str (.toRealPath path fs/empty-link-option-array))
             :exists true
             :is_file (.isRegularFile attrs)
             :is_directory (.isDirectory attrs)})
          {:path (str path)
           :exists false
           :is_file false
           :is_directory false})))))

(defn- make-ext-resource-attributes-fn [project]
  (rt/lua-fn ext-resource-attributes [{:keys [rt evaluation-context]} lua-resource-path]
    (let [proj-path (rt/->clj rt graph/resource-path-coercer lua-resource-path)]
      (if-let [resource (-> project
                            (project/workspace evaluation-context)
                            (workspace/find-resource proj-path evaluation-context))]
        (let [source-type (resource/source-type resource)]
          {:exists true
           :is_file (= :file source-type)
           :is_directory (= :folder source-type)})
        {:exists false
         :is_file false
         :is_directory false}))))

(def ^:private empty-lua-string
  (rt/->lua ""))

(def execute-last-arg-coercer
  (coerce/one-of
    coerce/string
    (coerce/hash-map :opt {:reload_resources coerce/boolean
                           :out (coerce/enum :capture :discard :pipe)
                           :err (coerce/enum :stdout :discard :pipe)})))

(defn- make-ext-execute-fn [^Path project-path display-output! reload-resources!]
  (rt/suspendable-lua-fn ext-execute [{:keys [rt]} & lua-args]
    (when (empty? lua-args)
      (throw (LuaError. "No arguments provided to editor.execute()")))
    (let [last-arg (rt/->clj rt execute-last-arg-coercer (last lua-args))
          butlast-args (mapv #(rt/->clj rt coerce/string %) (butlast lua-args))
          options-provided (map? last-arg)
          cmd+args (if options-provided butlast-args (conj butlast-args last-arg))
          options (if options-provided last-arg {})
          err (:err options :pipe)
          out (:out options :pipe)
          reload (:reload_resources options true)
          ^Process p (apply process/start!
                            {:dir (.toFile project-path)
                             :out (if (= out :capture) :pipe out)
                             :err err}
                            cmd+args)
          maybe-output-future (when (= :capture out)
                                (future/io
                                  (or (process/capture! (process/out p))
                                      empty-lua-string)))]
      (when (= :pipe out)
        (actions/input-stream->console (process/out p) display-output! :out))
      (when (= :pipe err)
        (actions/input-stream->console (process/err p) display-output! :err))
      (-> (.onExit p)
          (future/then
            (fn [_]
              (let [exit-code (.exitValue p)]
                (when-not (zero? exit-code)
                  (throw (LuaError. (format "Command \"%s\" exited with code %s"
                                            (string/join " " cmd+args)
                                            exit-code)))))))
          (cond-> maybe-output-future
                  (future/then (fn [_] maybe-output-future))

                  reload
                  (future/then
                    (fn [result]
                      (future/then
                        (reload-resources!)
                        (fn [_] (rt/and-refresh-context result))))))))))

(def bob-options-coercer
  (let [scalar-coercer (coerce/one-of coerce/string coerce/boolean coerce/integer)]
    (coerce/map-of
      (coerce/wrap-transform coerce/string string/replace \_ \-)
      (coerce/one-of scalar-coercer (coerce/vector-of scalar-coercer)))))

(def bob-options-or-command-coercer
  (coerce/one-of coerce/string bob-options-coercer))

(defn- make-ext-bob-fn [invoke-bob!]
  (rt/suspendable-lua-fn bob [{:keys [rt evaluation-context]} & lua-args]
    (let [[options commands] (if (empty? lua-args)
                               [{} []]
                               (let [options-or-command (rt/->clj rt bob-options-or-command-coercer (first lua-args))
                                     first-arg-is-command (string? options-or-command)
                                     options (if first-arg-is-command {} options-or-command)
                                     commands (into (if first-arg-is-command [options-or-command] [])
                                                    (map #(rt/->clj rt coerce/string %))
                                                    (rest lua-args))]
                                 [options commands]))]
      (invoke-bob! options commands evaluation-context))))

(defn- ensure-file-path-in-project-directory
  ^Path [^Path project-path ^String file-name]
  (let [normalized-path (.normalize (.resolve project-path file-name))]
    (if (.startsWith normalized-path project-path)
      normalized-path
      (throw (LuaError. (str "Can't access " file-name ": outside of project directory"))))))

(defn- make-ext-remove-file-fn [project-path reload-resources!]
  (rt/suspendable-lua-fn ext-remove-file [{:keys [rt]} lua-file-name]
    (let [file-name (rt/->clj rt coerce/string lua-file-name)
          file-path (ensure-file-path-in-project-directory project-path file-name)]
      (when-not (Files/exists file-path fs/empty-link-option-array)
        (throw (LuaError. (str "No such file or directory: " file-name))))
      (Files/delete file-path)
      (future/then (reload-resources!) rt/and-refresh-context))))

(def transaction-steps-coercer
  (coerce/vector-of
    (coerce/wrap-with-pred coerce/userdata #(= :transaction-step (:type (meta %))) "is not a transaction step")
    :min-count 1))

(def ^:private ext-transact
  (rt/suspendable-lua-fn ext-transact [{:keys [rt]} lua-txs]
    (let [txs (rt/->clj rt transaction-steps-coercer lua-txs)
          f (future/make)
          transact (bound-fn* g/transact)]
      (fx/on-fx-thread
        (try
          (transact txs)
          (future/complete! f (rt/and-refresh-context nil))
          (catch Throwable ex (future/fail! f ex))))
      f)))

(defn- make-ext-tx-set-fn [project]
  (rt/lua-fn ext-tx-set [{:keys [rt evaluation-context]} lua-node-id-or-path lua-property lua-value]
    (let [node-id (graph/node-id-or-path->node-id
                    (rt/->clj rt graph/node-id-or-path-coercer lua-node-id-or-path)
                    project
                    evaluation-context)
          property (rt/->clj rt coerce/string lua-property)
          setter (graph/ext-lua-value-setter node-id property rt project evaluation-context)]
      (if setter
        (-> (setter lua-value)
            (with-meta {:type :transaction-step})
            (rt/wrap-userdata "editor.tx.set(...)"))
        (throw (LuaError. (format "Can't set property \"%s\" of %s"
                                  property
                                  (name (graph/node-id->type-keyword node-id evaluation-context)))))))))

(defn- make-ext-save-fn [save!]
  (rt/suspendable-lua-fn ext-save [_]
    (future/then (save!) rt/and-refresh-context)))

(defn- make-open-resource-fn [workspace open-resource!]
  (rt/suspendable-lua-fn open-resource [{:keys [rt evaluation-context]} lua-resource-path]
    (let [resource-path (rt/->clj rt graph/resource-path-coercer lua-resource-path)
          resource (workspace/find-resource workspace resource-path evaluation-context)]
      (when (and resource (resource/exists? resource) (resource/openable? resource))
        (open-resource! resource)))))

(def ext-browse-fn
  (rt/suspendable-lua-fn browse [{:keys [rt]} lua-string]
    (let [s (rt/->clj rt coerce/string lua-string)]
      (future/io
        (try
          (.browse ui/desktop (URI. s))
          (catch Throwable e
            (throw (LuaError. ^String (ex-message e)))))))))

(def ext-open-external-file-fn
  (rt/suspendable-lua-fn open-external-file [{:keys [rt]} lua-string]
    (let [file-name (rt/->clj rt coerce/string lua-string)]
      (future/io
        (try
          (.open ui/desktop (io/file file-name))
          (catch Throwable e
            (throw (LuaError. ^String (ex-message e)))))))))

;; region json

(def json-decode-options-coercer
  (coerce/hash-map :opt {:all coerce/boolean}))

(def ext-json-decode
  (letfn [(decode [^LuaString lua-string opts]
            (with-open [r (-> lua-string .toInputStream io/reader (PushbackReader. 64))]
              (if (:all opts)
                (let [eof-value r]
                  (loop [acc (transient [])]
                    (let [ret (json/read r :eof-error? false :eof-value eof-value)]
                      (if (identical? ret eof-value)
                        (persistent! acc)
                        (recur (conj! acc ret))))))
                (json/read r))))]
    (rt/lua-fn ext-json-decode
      ([_ lua-string]
       (decode lua-string nil))
      ([{:keys [rt]} lua-string lua-options]
       (decode lua-string (rt/->clj rt json-decode-options-coercer lua-options))))))

(def json-value-coercer
  (coerce/recursive
    #(coerce/one-of
       coerce/null
       coerce/boolean
       coerce/number
       coerce/string
       (coerce/vector-of % :min-count 1)
       (coerce/map-of coerce/string %))))

(def ext-json-encode
  (rt/lua-fn ext-json-decode
    ([{:keys [rt]} lua-value]
     (json/write-str (rt/->clj rt json-value-coercer lua-value)))))

;; endregion

(def ext-project-binary-name
  (rt/lua-fn ext-project-binary-name [{:keys [rt]} lua-string]
    (BundleHelper/projectNameToBinaryName (rt/->clj rt coerce/string lua-string))))

(def ext-pprint
  (let [write-indent! (fn write-indent! [^PrintStream out ^long indent]
                        (loop [i 0]
                          (when-not (= i indent)
                            (.print out "  ")
                            (recur (inc i)))))]
    (rt/lua-fn ext-pprint [{:keys [rt]} & lua-values]
      (let [out (rt/stdout rt)]
        (run! (fn pprint
                ([v]
                 (pprint v 0 (HashSet.))
                 (.println out))
                ([^LuaValue v ^long indent ^HashSet seen]
                 (condp instance? v
                   LuaTable (if (.add seen v)
                              (let [array-length (.rawlen v)
                                    empty (and (zero? array-length)
                                               (.isnil (.arg1 (.next v LuaValue/NIL))))]
                                (if empty
                                  ;; for empty tables, print identity after closing brace
                                  (doto out
                                    (.print "{} --[[0x")
                                    (.print (Integer/toHexString (.hashCode v)))
                                    (.print "]]"))
                                  (do (.print out "{ --[[0x")
                                      (.print out (Integer/toHexString (.hashCode v)))
                                      (.println out "]]")
                                      (let [indent (inc indent)]
                                        ;; write array part
                                        (when (pos? array-length)
                                          (loop [i 1]
                                            (when-not (< array-length i)
                                              (write-indent! out indent)
                                              (pprint (.get v i) indent seen)
                                              (when-not (= i array-length)
                                                (.println out ","))
                                              (recur (inc i)))))
                                        ;; write hash part
                                        (loop [prev-k LuaValue/NIL
                                               prefix-with-comma (pos? array-length)]
                                          (let [kv-varargs (.next v prev-k)
                                                k (.arg1 kv-varargs)]
                                            (when-not (.isnil k)
                                              ;; skip array keys
                                              (if (and (.isint k) (<= (.toint k) array-length))
                                                (recur k prefix-with-comma)
                                                (do
                                                  (when prefix-with-comma
                                                    (.println out ","))
                                                  (write-indent! out indent)
                                                  (if (and (.isstring k)
                                                           (re-matches #"^[a-zA-Z_][a-zA-Z0-9_]*$" (str k)))
                                                    (.print out (str k))
                                                    (do
                                                      (.print out "[")
                                                      (pprint k indent seen)
                                                      (.print out "]")))
                                                  (.print out " = ")
                                                  (pprint (.arg kv-varargs 2) indent seen)
                                                  ;; wrap `true` in boolean so that the loop compiles, otherwise it complains
                                                  ;; about java.lang.Boolean not matching primitive boolean ¯\_(ツ)_/¯
                                                  (recur k (boolean true))))))))
                                      (.println out)
                                      (write-indent! out indent)
                                      (.print out "}"))))
                              ;; write previously seen table
                              (doto out
                                (.print "<table: 0x")
                                (.print (Integer/toHexString (.hashCode v)))
                                (.print ">")))
                   LuaFunction (doto out
                                 (.print "<")
                                 (.print (str v))
                                 (.print ">"))
                   LuaString (.print out (pr-str (str v)))
                   (.print out (str v)))))
              lua-values)))))

;; region http

(def http-request-options-coercer
  (coerce/one-of
    (coerce/hash-map
      :opt {:method coerce/string
            :headers (coerce/map-of coerce/string coerce/string)
            :body coerce/string
            :as (coerce/enum :string :json)}
      :extra-keys false)
    coerce/null))

(def ext-http-request
  (rt/suspendable-lua-fn ext-http-request
    ([ctx lua-url]
     (ext-http-request ctx lua-url nil))
    ([{:keys [rt]} lua-url maybe-lua-options]
     (let [options (some->> maybe-lua-options (rt/->clj rt http-request-options-coercer))
           json (= :json (:as options))]
       (try
         (-> (http/request
               (rt/->clj rt coerce/string lua-url)
               (cond-> options json (assoc :as :input-stream)))
             (future/then
               (fn http-request-then [response]
                 (cond-> response json (assoc :body (with-open [reader (io/reader (:body response))]
                                                      (json/read reader))))))
             (future/catch
               (fn http-request-catch [e]
                 (throw (LuaError. (str (or (ex-message e) (.getSimpleName (class e)))))))))
         ;; we might get an exception when parsing the URI before we start the async request execution
         (catch Throwable e
           (throw (LuaError. (str (or (ex-message e) (.getSimpleName (class e))))))))))))

;; endregion

;; endregion

;; region language servers

(defn- built-in-language-servers []
  (let [lua-lsp-root (str (system/defold-unpack-path) "/" (.getPair (Platform/getHostPlatform)) "/bin/lsp/lua")]
    #{{:languages #{"lua"}
       :watched-files [{:pattern "**/.luacheckrc"}]
       :launcher {:command [(str lua-lsp-root "/bin/lua-language-server" (when (os/is-win32?) ".exe"))
                            (str "--configpath=" lua-lsp-root "/config.json")]}}}))

(def language-servers-coercer
  (coerce/vector-of
    (coerce/hash-map
      :req {:languages (coerce/vector-of coerce/string :distinct true :min-count 1)
            :command (coerce/vector-of coerce/string :min-count 1)}
      :opt {:watched_files (coerce/vector-of (coerce/hash-map :req {:pattern coerce/string}) :min-count 1)})))

(defn- reload-language-servers! [project state evaluation-context]
  (let [{:keys [display-output! rt]} state
        lsp (lsp/get-node-lsp project)]
    (lsp/set-servers!
      lsp
      (into
        (built-in-language-servers)
        (comp
          (mapcat
            (fn [[path lua-language-servers]]
              (error-handling/try-with-extension-exceptions
                :display-output! display-output!
                :label (str "Reloading language servers in " path)
                :catch []
                (rt/->clj rt language-servers-coercer lua-language-servers))))
          (map (fn [language-server]
                 (-> language-server
                     (set/rename-keys {:watched_files :watched-files})
                     (update :languages set)
                     (dissoc :command)
                     (assoc :launcher (select-keys language-server [:command]))))))
        (execute-all-top-level-functions state :get_language_servers {} evaluation-context)))))

;; endregion

;; region reload

(def commands-coercer
  (coerce/vector-of
    (coerce/hash-map
      :req {:label coerce/string
            :locations (coerce/vector-of
                         (coerce/enum "Edit" "View" "Assets" "Outline" "Bundle")
                         :distinct true
                         :min-count 1)}
      :opt {:query (coerce/hash-map
                     :opt {:selection (coerce/hash-map
                                        :req {:type (coerce/enum :resource :outline)
                                              :cardinality (coerce/enum :one :many)})
                           :argument (coerce/const true)})
            :id prefs-docs/serializable-keyword-coercer
            :active coerce/function
            :run coerce/function})))

(defn- reload-commands! [project state evaluation-context]
  (let [{:keys [display-output! rt]} state]
    (handler/register! ::commands
      :handlers
      (into []
            (mapcat
              (fn [[path lua-ret]]
                (error-handling/try-with-extension-exceptions
                  :display-output! display-output!
                  :label (str "Reloading commands in " path)
                  :catch nil
                  (eduction
                    (keep (fn [command]
                            (error-handling/try-with-extension-exceptions
                              :display-output! display-output!
                              :label (str (:label command) " in " path)
                              :catch nil
                              (commands/command->dynamic-handler command path project state))))
                    (rt/->clj rt commands-coercer lua-ret)))))
            (execute-all-top-level-functions state :get_commands {} evaluation-context)))))

(defn- reload-prefs! [project-path state evaluation-context]
  (let [{:keys [display-output! rt]} state
        report-omitted-schema! (fn report-omitted-schema! [path reason]
                                 (display-output! :err (str "Omitting prefs schema definition for path '" (string/join "." (map name path)) "': " reason)))
        omit-on-conflict (fn omit-on-conflict [a b path]
                           (if (= a b)
                             a
                             (do (report-omitted-schema! path "conflicts with another editor script schema")
                                 nil)))
        schema (->> (execute-all-top-level-functions state :get_prefs_schema nil evaluation-context)
                    (e/keep
                      (fn [[proj-path lua-ret]]
                        (error-handling/try-with-extension-exceptions
                          :display-output! display-output!
                          :label (str "Reloading prefs schema in " proj-path)
                          :catch nil
                          (prefs/subtract-schemas
                            (fn [_ _ path]
                              (report-omitted-schema! path (str "'" proj-path "' defines a schema that conflicts with the editor schema")))
                            (prefs-functions/lua-schema-definition->schema rt lua-ret)
                            prefs/default-schema))))
                    (reduce
                      (fn [a b]
                        (if a
                          (prefs/merge-schemas omit-on-conflict a b)
                          b))
                      nil))]
    (when schema
      (prefs/register-project-schema! project-path schema))))

(defn- add-all-entry [m path module]
  (reduce-kv
    (fn [acc k v]
      (update acc k (fnil conj []) [path v]))
    m
    module))

(def hooks-file-path "/hooks.editor_script")

(def module-coercer
  (coerce/hash-map :opt {:get_commands coerce/function
                         :get_language_servers coerce/function
                         :get_prefs_schema coerce/function
                         :on_build_started coerce/function
                         :on_build_finished coerce/function
                         :on_bundle_started coerce/function
                         :on_bundle_finished coerce/function
                         :on_target_launched coerce/function
                         :on_target_terminated coerce/function}))

(def ^:private bundle-editor-script-prototype
  (rt/read (io/resource "bundle.editor_script") "bundle.editor_script"))

(defn- re-create-ext-state [initial-state evaluation-context]
  (let [{:keys [rt display-output!]} initial-state]
    (->> (e/concat
           [bundle-editor-script-prototype]
           (:library-prototypes initial-state)
           (:project-prototypes initial-state))
         (reduce
           (fn [acc x]
             (cond
               (instance? LuaError x)
               (do
                 (display-output! :err (str "Compilation failed" (some->> (ex-message x) (str ": "))))
                 acc)

               (instance? Prototype x)
               (let [proto-path (.tojstring (.-source ^Prototype x))]
                 (if-let [module (error-handling/try-with-extension-exceptions
                                   :display-output! display-output!
                                   :label (str "Loading " proto-path)
                                   :catch nil
                                   (rt/->clj rt module-coercer (rt/invoke-immediate-1 rt (rt/bind rt x) evaluation-context)))]
                   (-> acc
                       (update :all add-all-entry proto-path module)
                       (cond-> (= hooks-file-path proto-path)
                               (assoc :hooks module)))
                   acc))

               (nil? x)
               acc

               :else
               (throw (ex-info (str "Unexpected prototype value: " x) {:prototype x}))))
           initial-state))))

(defn line-writer [f]
  (let [sb (StringBuilder.)]
    (PrintWriter-on #(doseq [^char ch %]
                       (if (= \newline ch)
                         (let [str (.toString sb)]
                           (.delete sb 0 (.length sb))
                           (f str))
                         (.append sb ch)))
                    nil)))

(defn- find-resource [project {:keys [evaluation-context]} proj-path]
  (when-let [node (project/get-resource-node project proj-path evaluation-context)]
    (data/lines-input-stream (g/node-value node :lines evaluation-context))))

(defn- resolve-file [^Path project-path _ ^String file-name]
  (str (ensure-file-path-in-project-directory project-path file-name)))

(def ^:private prelude-prototype (rt/read (io/resource "prelude.lua") "prelude.lua"))

;; endregion

;; region public API

(defn reload!
  "Reload the extensions

  Args:
    project    the project node id
    kind       which scripts to reload, either :all, :library or :project

  Required kv-args:
    :prefs                editor prefs
    :reload-resources!    0-arg function that asynchronously reloads the editor
                          resources, returns a CompletableFuture (that might
                          complete exceptionally if reload fails)
    :display-output!      2-arg function used for displaying output in the
                          console, the args are:
                            type    output type, :out or :err
                            msg     a string to output, might be multiline
    :save!                0-arg function that asynchronously saves any unsaved
                          changes, returns CompletableFuture (that might
                          complete exceptionally if reload fails)
    :open-resource!       1-arg function that asynchronously opens the supplied
                          resource either in an editor tab or in another app that
                          has OS-defined file association, returns
                          CompletableFuture (that might complete exceptionally
                          if resource could not be opened)
    :invoke-bob!          3-arg function that asynchronously invokes bob and
                          returns a CompletableFuture (which may complete
                          exceptionally if bob invocation fails). The args:
                            options               bob options, a map from string
                                                  to bob option value, see
                                                  editor.pipeline.bob/invoke!
                            commands              bob commands, vector of
                                                  strings
                            evaluation-context    evaluation context of the
                                                  invocation"
  [project kind & {:keys [prefs reload-resources! display-output! save! open-resource! invoke-bob!] :as opts}]
  {:pre [prefs reload-resources! display-output! save! open-resource! invoke-bob!]}
  (g/with-auto-evaluation-context evaluation-context
    (let [extensions (g/node-value project :editor-extensions evaluation-context)
          old-state (ext-state project evaluation-context)
          workspace (project/workspace project evaluation-context)
          project-path (.toPath (workspace/project-path workspace evaluation-context))
          rt (rt/make
               :find-resource (partial find-resource project)
               :resolve-file (partial resolve-file project-path)
               :close-written (rt/suspendable-lua-fn [_]
                                (future/then (reload-resources!) rt/and-refresh-context))
               :out (line-writer #(display-output! :out %))
               :err (line-writer #(display-output! :err %))
               :env {"editor" {"bundle" {"project_binary_name" ext-project-binary-name} ;; undocumented, hidden API!
                               "get" (make-ext-get-fn project)
                               "can_get" (make-ext-can-get-fn project)
                               "can_set" (make-ext-can-set-fn project)
                               "create_directory" (make-ext-create-directory-fn project reload-resources!)
                               "delete_directory" (make-ext-delete-directory-fn project reload-resources!)
                               "resource_attributes" (make-ext-resource-attributes-fn project)
                               "external_file_attributes" (make-ext-external-file-attributes-fn project-path)
                               "execute" (make-ext-execute-fn project-path display-output! reload-resources!)
                               "bob" (make-ext-bob-fn invoke-bob!)
                               "browse" ext-browse-fn
                               "open_external_file" ext-open-external-file-fn
                               "platform" (.getPair (Platform/getHostPlatform))
                               "prefs" (prefs-functions/env prefs)
                               "save" (make-ext-save-fn save!)
                               "transact" ext-transact
                               "tx" {"set" (make-ext-tx-set-fn project)}
                               "ui" (assoc
                                      (ui-components/env workspace project project-path)
                                      "open_resource" (make-open-resource-fn workspace open-resource!))
                               "version" (system/defold-version)
                               "engine_sha1" (system/defold-engine-sha1)
                               "editor_sha1" (system/defold-editor-sha1)}
                     "http" {"request" ext-http-request}
                     "json" {"decode" ext-json-decode
                             "encode" ext-json-encode}
                     "io" {"tmpfile" nil}
                     "os" {"execute" nil
                           "exit" nil
                           "remove" (make-ext-remove-file-fn project-path reload-resources!)
                           "rename" nil
                           "setlocale" nil
                           "tmpname" nil}
                     "pprint" ext-pprint
                     "zip" (zip/env project-path reload-resources!)})
          _ (rt/invoke-immediate rt (rt/bind rt prelude-prototype) evaluation-context)
          new-state (re-create-ext-state
                      (assoc opts
                        :rt rt
                        :library-prototypes (if (or (= :all kind) (= :library kind))
                                              (g/node-value extensions :library-prototypes evaluation-context)
                                              (:library-prototypes old-state []))
                        :project-prototypes (if (or (= :all kind) (= :project kind))
                                              (g/node-value extensions :project-prototypes evaluation-context)
                                              (:project-prototypes old-state [])))
                      evaluation-context)]
      (g/user-data-swap! extensions :state (constantly new-state))
      (reload-prefs! project-path new-state evaluation-context)
      (reload-language-servers! project new-state evaluation-context)
      (reload-commands! project new-state evaluation-context)
      nil)))

(defn- hook-exception->error [^Throwable ex project hook-keyword]
  (let [^Throwable root (stacktrace/root-cause ex)
        message (ex-message root)
        [_ file line :as match] (re-find console/line-sub-regions-pattern message)]
    (g/map->error
      (cond-> {:_node-id (or (when match (project/get-resource-node project file))
                             (project/get-resource-node project hooks-file-path))
               :message (str (name hook-keyword) " in " hooks-file-path " failed: " message)
               :severity :fatal}

              line
              (assoc-in [:user-data :cursor-range]
                        (data/line-number->CursorRange (Integer/parseInt line)))))))

(defn execute-hook!
  "Execute hook defined in this project

  Returns a CompletableFuture that will finish when the hook processing is
  finished. If the hook execution fails, the error will be reported to the user
  and the future will be completed as per exception policy.

  Args:
    project         the project node id
    hook-keyword    keyword like :on_build_started
    opts            an object that will be serialized and passed to the Lua
                    hook function. WARNING: all node ids should be wrapped with
                    vm/wrap-user-data since Lua numbers lack necessary precision

  Optional kv-args:
    :exception-policy    what to do if the hook eventually fails, can be either:
                           :as-error    transform exception to error value
                                        suitable for the graph
                           :ignore      return nil
                         When not provided, the exception will be re-thrown"
  [project hook-keyword opts & {:keys [exception-policy]}]
  (g/with-auto-evaluation-context evaluation-context
    (let [{:keys [rt display-output! hooks] :as state} (ext-state project evaluation-context)]
      (if-let [lua-fn (get hooks hook-keyword)]
        (-> (rt/invoke-suspending-1 rt lua-fn (rt/->lua opts))
            (future/then
              (fn [lua-result]
                (when-not (rt/coerces-to? rt coerce/null lua-result)
                  (lsp.async/with-auto-evaluation-context evaluation-context
                    (actions/perform! lua-result project state evaluation-context)))))
            (future/catch
              (fn [ex]
                (error-handling/display-script-error! display-output! (str "hook " (name hook-keyword)) ex)
                (case exception-policy
                  :as-error (hook-exception->error ex project hook-keyword)
                  :ignore nil
                  (throw ex)))))
        (future/completed nil)))))

;; endregion
