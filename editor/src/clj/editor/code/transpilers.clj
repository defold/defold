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

(ns editor.code.transpilers
  (:require [clojure.java.io :as io]
            [clojure.set :as set]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code.resource :as r]
            [editor.code.script-compilation :as script-compilation]
            [editor.code.util :as code.util]
            [editor.core :as core]
            [editor.dialogs :as dialogs]
            [editor.fs :as fs]
            [editor.graph-util :as gu]
            [editor.resource :as resource]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [internal.java :as java]
            [internal.util :as util]
            [service.log :as log]
            [util.coll :refer [pair pair-map-by]])
  (:import [com.defold.extension.pipeline ILuaTranspiler ILuaTranspiler$Issue ILuaTranspiler$Severity]
           [com.dynamo.bob ClassLoaderScanner]
           [java.io File]
           [org.apache.commons.io FilenameUtils]))

(defn- transpiler-issue->error-value [proj-path->node-id ^ILuaTranspiler$Issue issue]
  (let [node-id (proj-path->node-id (.-resourcePath issue))]
    (g/map->error
      (cond-> {:_label :modified-lines
               :severity (condp = (.-severity issue)
                           ILuaTranspiler$Severity/INFO :info
                           ILuaTranspiler$Severity/WARNING :warning
                           ILuaTranspiler$Severity/ERROR :fatal)
               :message (.-message issue)
               :user-data (r/make-code-error-user-data (.-resourcePath issue) (.-lineNumber issue))}
              node-id
              (assoc :_node-id node-id)))))

(defn- ->fatal-error [node-id issues]
  (let [aggregate (g/error-aggregate issues :_node-id node-id :_label :modified-lines)]
    (when (= :fatal (:severity aggregate))
      aggregate)))

(defn- make-temporary-compile-directory! [transpiler all-source-save-datas]
  (let [dir (fs/create-temp-directory! (str "tr-" (.getSimpleName (class transpiler))))]
    (run! (fn [{:keys [resource content dirty?]}]
            (let [file (io/file dir (resource/path resource))]
              (io/make-parents file)
              (if content
                (spit file content)
                (with-open [is (io/input-stream resource)]
                  (io/copy is file)))
              (when-not dirty?
                (.setLastModified file (.lastModified (io/file resource))))))
          all-source-save-datas)
    dir))

(g/defnk produce-build-output [^ILuaTranspiler instance build-file-save-data source-code-save-datas root lua-preprocessors]
  (when (and build-file-save-data (pos? (count source-code-save-datas)))
    (let [build-file-node-id (:node-id build-file-save-data)
          all-sources (conj source-code-save-datas build-file-save-data)
          workspace (resource/workspace (:resource build-file-save-data))
          proj-path->node-id (pair-map-by (comp resource/proj-path :resource) :node-id all-sources)
          use-project-dir (every? (fn [{:keys [resource dirty?]}]
                                    (and (not dirty?) (resource/file-resource? resource)))
                                  all-sources)
          source-dir (if use-project-dir
                       (io/file root)
                       (make-temporary-compile-directory! instance all-sources))
          plugin-dir (io/file root (subs workspace/plugins-dir 1))
          output-dir (io/file root "build" "tr" (.getSimpleName (class instance)))]
      (fs/create-directories! output-dir)
      (try
        (let [issues (.transpile instance plugin-dir source-dir output-dir)]
          (or (->> issues
                   (mapv #(transpiler-issue->error-value proj-path->node-id %))
                   (->fatal-error build-file-node-id))
              (into {}
                    (keep
                      (fn [^File file]
                        (when (= "lua" (FilenameUtils/getExtension (.getName file)))
                          (let [resource (resource/make-file-resource workspace (str output-dir) file [] (constantly false))
                                build-targets (script-compilation/build-targets build-file-node-id resource (code.util/split-lines (slurp resource)) lua-preprocessors [] [] proj-path->node-id)]
                            (pair (resource/proj-path resource) build-targets)))))
                    (fs/file-walker output-dir))))
        (catch Exception e
          (g/->error build-file-node-id :modified-lines :fatal (:resource build-file-save-data)
                     (str "Compilation failed: " (ex-message e))))
        (finally
          (when-not use-project-dir
            (fs/delete-directory! source-dir {:fail :silently})))))))

(g/defnode TranspilerNode
  (property build-file-proj-path g/Str)
  (property instance ILuaTranspiler)
  (input root g/Str)
  (input build-file-save-data g/Any)
  (input source-code-save-datas g/Any :array)
  (input lua-preprocessors g/Any)
  (output transpiler-info g/Any (g/fnk [_node-id build-file-proj-path instance :as info] info))
  (output build-output g/Any :cached produce-build-output))

(g/defnode CodeTranspilersNode
  (inherits core/Scope)
  (input transpiler-infos g/Any :array)
  (input build-outputs g/Any :array)
  (input lua-preprocessors g/Any)
  (output lua-preprocessors g/Any (gu/passthrough lua-preprocessors))
  (output transpiler-infos g/Any (gu/passthrough transpiler-infos))
  (output build-file-proj-path->transpiler-node-id g/Any :cached
          (g/fnk [transpiler-infos]
            (case (count transpiler-infos)
              0 (constantly nil)
              1 (let [{:keys [build-file-proj-path _node-id]} (first transpiler-infos)]
                  (fn [proj-path]
                    (when (= proj-path build-file-proj-path) _node-id)))
              (pair-map-by :build-file-proj-path :_node-id transpiler-infos))))
  (output build-output g/Any :cached
          (g/fnk [build-outputs]
            (persistent! (reduce conj! (transient {}) build-outputs)))))

(g/defnode SourceNode
  (inherits r/CodeEditorResourceNode))

(defn- report-error! [error-message faulty-class-names]
  (ui/run-later
    (dialogs/make-info-dialog
      {:title "Unable to Load Plugin"
       :size :large
       :icon :icon/triangle-error
       :always-on-top true
       :header error-message
       :content (string/join "\n"
                             (concat
                               ["The following classes from editor plugins are not compatible with this version of the editor:"
                                ""]
                               (mapv dialogs/indent-with-bullet (sort faulty-class-names))
                               [""
                                "The project might not build without them."
                                "Please edit your project dependencies to refer to a suitable version."]))})))

(defn- initialize-lua-transpiler-classes [^ClassLoader class-loader]
  (let [{:keys [classes faulty-class-names]}
        (->> (ClassLoaderScanner/scanClassLoader class-loader "com.defold.extension.pipeline")
             (eduction
               (keep
                 (fn [^String class-name]
                   (try
                     (let [uninitialized-class (Class/forName class-name false class-loader)]
                       (when (java/public-implementation? uninitialized-class ILuaTranspiler)
                         (let [initialized-class (Class/forName class-name true class-loader)]
                           (pair :classes initialized-class))))
                     (catch Exception e
                       (log/error :msg (str "Exception in static initializer of ILuaTranspiler implementation "
                                            class-name ": " (.getMessage e))
                                  :exception e)
                       (pair :faulty-class-names class-name))))))
             (util/group-into {} #{} key val))]
    (when faulty-class-names
      (throw (ex-info "Failed to initialize Lua transpiler plugins" {:faulty-class-names faulty-class-names})))
    (or classes #{})))

(defn- create-lua-transpilers [transpiler-classes]
  (let [{:keys [transpilers faulty-class-names]}
        (->> transpiler-classes
             (eduction
               (map (fn [^Class transpiler-plugin-class]
                      (try
                        (let [^ILuaTranspiler transpiler (java/invoke-no-arg-constructor transpiler-plugin-class)
                              build-file-proj-path (.getBuildFileResourcePath transpiler)
                              source-ext (.getSourceExt transpiler)]
                          (when (or (not (string? build-file-proj-path))
                                    (not (string/starts-with? build-file-proj-path "/")))
                            (throw (Exception. (str "Invalid build file resource path: " build-file-proj-path))))
                          (when (or (not (string? source-ext))
                                    (string/starts-with? source-ext "."))
                            (throw (Exception. (str "Invalid source extension: " source-ext))))
                          (pair :transpilers {:build-file-proj-path build-file-proj-path
                                              :source-ext source-ext
                                              :instance transpiler}))
                        (catch Exception e
                          (let [class-name (.getSimpleName transpiler-plugin-class)]
                            (log/error :msg (str "Failed to initialize transpiler " class-name) :exception e)
                            (pair :faulty-class-names class-name)))))))
             (util/group-into {} [] key val))]
    (when faulty-class-names
      (throw (ex-info "Failed to create Lua transpiler plugins" {:faulty-class-names faulty-class-names})))
    transpilers))

(defn reload-lua-transpilers! [code-transpilers workspace class-loader]
  (try
    (let [old-transpiler-class->node-id (pair-map-by
                                          #(-> % :instance class)
                                          :_node-id
                                          (g/node-value code-transpilers :transpiler-infos))
          old-transpiler-classes (set (keys old-transpiler-class->node-id))
          new-transpiler-classes (initialize-lua-transpiler-classes class-loader)]
      (when-not (= old-transpiler-classes new-transpiler-classes)
        (let [removed (set/difference old-transpiler-classes new-transpiler-classes)
              added (set/difference new-transpiler-classes old-transpiler-classes)]
          (g/transact
            (for [removed-class removed]
              (g/delete-node (old-transpiler-class->node-id removed-class)))
            (for [{:keys [source-ext build-file-proj-path instance]} (create-lua-transpilers added)]
              (g/make-nodes (g/node-id->graph-id code-transpilers) [transpiler TranspilerNode]
                (r/register-code-resource-type
                  workspace
                  :ext source-ext
                  :icon "icons/32/Icons_12-Script-type.png"
                  :node-type SourceNode
                  :view-types [:code :default]
                  :additional-load-fn (fn [_ self _]
                                        (g/connect self :save-data transpiler :source-code-save-datas)))
                (g/set-property transpiler :build-file-proj-path build-file-proj-path :instance instance)
                (g/connect code-transpilers :lua-preprocessors transpiler :lua-preprocessors)
                (g/connect workspace :root transpiler :root)
                (g/connect transpiler :_node-id code-transpilers :nodes)
                (g/connect transpiler :transpiler-info code-transpilers :transpiler-infos)
                (g/connect transpiler :build-output code-transpilers :build-outputs))))))
      nil)
    (catch Exception e
      (report-error! (ex-message e) (:faulty-class-names (ex-data e))))))

(defn load-build-file-transaction-step [code-transpilers resource-node-id proj-path]
  (let [f (g/node-value code-transpilers :build-file-proj-path->transpiler-node-id)]
    (when-let [transpiler-node-id (f proj-path)]
      (g/connect resource-node-id :save-data transpiler-node-id :build-file-save-data))))

(defn build-output [code-transpilers evaluation-context]
  (g/node-value code-transpilers :build-output evaluation-context))
