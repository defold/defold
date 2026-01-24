;; Copyright 2020-2026 The Defold Foundation
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

(ns editor.workspace
  "Define the concept of a project, and its Project node type. This namespace bridges between Eclipse's workbench and
ordinary paths."
  (:require [clojure.edn :as edn]
            [clojure.java.io :as io]
            [clojure.set :as set]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code.preprocessors :as code.preprocessors]
            [editor.dialogs :as dialogs]
            [editor.fs :as fs]
            [editor.library :as library]
            [editor.localization :as localization]
            [editor.notifications :as notifications]
            [editor.prefs :as prefs]
            [editor.progress :as progress]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-watch :as resource-watch]
            [editor.ui :as ui]
            [editor.url :as url]
            [editor.util :as util]
            [internal.java :as java]
            [internal.util :as iutil]
            [schema.core :as s]
            [service.log :as log]
            [util.coll :as coll :refer [pair]]
            [util.digest :as digest]
            [util.eduction :as e]
            [util.fn :as fn]
            [util.path :as path])
  (:import [clojure.lang DynamicClassLoader]
           [editor.resource FileResource]
           [com.dynamo.bob Platform]
           [editor.resource FileResource]
           [java.io File FileNotFoundException IOException PushbackReader]
           [java.net URI]
           [org.apache.commons.io FilenameUtils]))

(set! *warn-on-reflection* true)

(def build-dir "/build/default/")
(def build-html5-dir "/build/default_html5/")
(def plugins-dir "/build/plugins/")

;; SDK api
(def load-class! java/load-class!)

(defn project-directory
  "Returns a File representing the canonical path of the project directory."
  (^File [workspace]
   (resource/project-directory (g/unsafe-basis) workspace))
  (^File [basis workspace]
   (resource/project-directory basis workspace)))

(defn code-preprocessors
  ([workspace]
   (g/with-auto-evaluation-context evaluation-context
     (code-preprocessors workspace evaluation-context)))
  ([workspace evaluation-context]
   (g/node-value workspace :code-preprocessors evaluation-context)))

(defn notifications
  ([workspace]
   (g/with-auto-evaluation-context evaluation-context
     (notifications workspace evaluation-context)))
  ([workspace evaluation-context]
   (g/node-value workspace :notifications evaluation-context)))

(defn localization [workspace evaluation-context]
  (g/node-value workspace :localization evaluation-context))

(defn- skip-first-char [path]
  (subs path 1))

(defn build-path
  (^File [workspace]
   (io/file (project-directory workspace) (skip-first-char build-dir)))
  (^File [workspace build-resource-path]
   (io/file (build-path workspace) (skip-first-char build-resource-path))))

(defn build-html5-path
  (^File [workspace]
   (io/file (project-directory workspace) (skip-first-char build-html5-dir))))

(defn plugin-path
  (^File [workspace]
   (io/file (project-directory workspace) (skip-first-char plugins-dir)))
  (^File [workspace path]
   (io/file (project-directory workspace) (str (skip-first-char plugins-dir) (skip-first-char path)))))

(defn as-proj-path
  (^String [workspace file-or-path]
   (as-proj-path (g/now) workspace file-or-path))
  (^String [basis workspace file-or-path]
   (let [file (io/as-file file-or-path)
         project-directory (project-directory basis workspace)]
     (when (fs/below-directory? file project-directory)
       (resource/file->proj-path project-directory file)))))

(defrecord BuildResource [resource prefix]
  resource/Resource
  (children [this] nil)
  (ext [this] (:build-ext (resource/resource-type this) "unknown"))
  (resource-type [this] (resource/resource-type resource))
  (source-type [this] (resource/source-type resource))
  (read-only? [this] false)
  (path [this] (let [ext (resource/ext this)
                     ext (if (not-empty ext) (str "." ext) "")]
                 (if-let [path (resource/path resource)]
                   (str (FilenameUtils/removeExtension path) ext)
                   (let [suffix (format "%x" (resource/resource-hash this))]
                     (str prefix "_generated_" suffix ext)))))
  (abs-path [this] (.getAbsolutePath (io/file (build-path (resource/workspace this)) (resource/path this))))
  (proj-path [this] (str "/" (resource/path this)))
  (resource-name [this] (resource/resource-name resource))
  (workspace [this] (resource/workspace resource))
  (resource-hash [this] (resource/resource-hash resource))
  (openable? [this] false)
  (editable? [this] false)
  (loaded? [this] false)

  io/IOFactory
  (make-input-stream [this opts] (io/make-input-stream (File. (resource/abs-path this)) opts))
  (make-reader [this opts] (io/make-reader (io/make-input-stream this opts) opts))
  (make-output-stream [this opts] (let [file (File. (resource/abs-path this))] (io/make-output-stream file opts)))
  (make-writer [this opts] (io/make-writer (io/make-output-stream this opts) opts))

  io/Coercions
  (as-file [this] (File. (resource/abs-path this)))

  path/Coercions
  (as-path [this] (path/as-path (resource/abs-path this))))

(defmethod print-method BuildResource [build-resource ^java.io.Writer w]
  ;; Avoid evaluating resource-type, since it requires a live system. As a
  ;; result, the file extension will be taken from the source-resource, and our
  ;; :build-ext will be ignored.
  (let [source-resource (:resource build-resource)]
    (->> (or (resource/proj-path source-resource)
             (let [prefix (or (:prefix build-resource) "")
                   suffix (format "%x" (resource/resource-hash source-resource))
                   source-ext (resource/ext source-resource)]
               (format "/%s_generated_%s.%s" prefix suffix source-ext)))
         (pr-str)
         (format "{:BuildResource %s}")
         (.write w))))

(def build-resource? (partial instance? BuildResource))

(defn source-resource? [value]
  (and (resource/resource? value)
       (not (build-resource? value))))

(defn make-build-resource
  ([source-resource]
   (make-build-resource source-resource nil))
  ([source-resource prefix]
   {:pre [(source-resource? source-resource)]}
   (BuildResource. source-resource prefix)))

(defn counterpart-build-resource
  "Given a BuildResource, returns its editable or non-editable counterpart if
  applicable. We use this during build target fusion to ensure embedded
  resources from editable resources are fused with the equivalent embedded
  resources from non-editable resources. Returns nil if the BuildResource has no
  counterpart applicable to build target fusion."
  [build-resource]
  {:pre [(build-resource? build-resource)]}
  (let [source-resource (:resource build-resource)]
    (assert (source-resource? source-resource))
    (when (resource/memory-resource? source-resource)
      (assoc build-resource
        :resource (resource/counterpart-memory-resource source-resource)))))

(defn sort-resource-tree [{:keys [children] :as tree}]
  (let [sorted-children (->> children
                             (map sort-resource-tree)
                             (sort
                               (util/comparator-chain
                                 (util/comparator-on editor.resource/file-resource?)
                                 (util/comparator-on #({:folder 0 :file 1} (editor.resource/source-type %)))
                                 (util/comparator-on util/natural-order editor.resource/resource-name)))
                             vec)]
    (assoc tree :children sorted-children)))

(g/defnk produce-resource-tree [_node-id root resource-snapshot editable-proj-path? unloaded-proj-path?]
  (sort-resource-tree
    (resource/make-file-resource _node-id root (io/as-file root) (:resources resource-snapshot) editable-proj-path? unloaded-proj-path?)))

(g/defnk produce-resource-list [resource-tree]
  (vec (sort-by resource/proj-path util/natural-order (resource/resource-seq resource-tree))))

(g/defnk produce-resource-map [resource-list]
  (into {}
        (map #(pair (resource/proj-path %) %))
        resource-list))

(defn get-view-type
  ([workspace id]
   (g/with-auto-evaluation-context evaluation-context
     (get-view-type workspace id evaluation-context)))
  ([workspace id evaluation-context]
   (get (g/node-value workspace :view-types evaluation-context) id)))

(defn- editor-openable-view-type? [view-type]
  (case view-type
    (:default :text) false
    true))

(defn- make-editable-resource-type-merge-fn [prioritized-editable]
  {:pre [(boolean? prioritized-editable)]}
  (fn editable-resource-type-merge-fn [old-resource-type new-resource-type]
    (let [old-editable (:editable old-resource-type)
          new-editable (:editable new-resource-type)]
      (assert (boolean? old-editable))
      (assert (boolean? new-editable))
      (if (or (= old-editable new-editable)
              (= prioritized-editable new-editable))
        new-resource-type
        old-resource-type))))

(defn- make-editable-resource-type-map-update-fn [prioritized-editable]
  (let [editable-resource-type-merge-fn (make-editable-resource-type-merge-fn prioritized-editable)]
    (fn editable-resource-type-map-update-fn [resource-type-map updated-resource-types-by-ext]
      (merge-with editable-resource-type-merge-fn resource-type-map updated-resource-types-by-ext))))

(def ^:private editable-resource-type-map-update-fn (make-editable-resource-type-map-update-fn true))
(def ^:private non-editable-resource-type-map-update-fn (make-editable-resource-type-map-update-fn false))

(defn- default-search-value-fn [node-id _resource evaluation-context]
  (g/node-value node-id :save-value evaluation-context))

(defn register-resource-type
  "Register new resource type to be handled by the editor

  Required kv-args:
    :ext    file extension associated with the resource type, either a string
            or a coll of strings

  Optional kv-args:
    :node-type          a loaded resource node type
    :textual?           whether the resource is textual, default false. This
                        flag affects search in files availability for the
                        resource type and lf/crlf handling on save
    :language           language identifier string used for textual resources
                        that can be opened as code, used for LSP interactions,
                        see https://code.visualstudio.com/docs/languages/identifiers#_known-language-identifiers
                        for common values; defaults to \"plaintext\"
    :build-ext          file extension of a built resource, defaults to :ext's
                        value with appended \"c\"
    :dependencies-fn    fn of node's :source-value output to a collection of
                        resource project paths that this node depends on,
                        affects loading order
    :load-fn            a function from project, new node id and resource to
                        transaction step, invoked on loading the resource of
                        the type; default editor.placeholder-resource/load-node
    :read-fn            a fn from clojure.java.io/reader-able object (e.g.
                        a resource or a Reader) to a data structure
                        representation of the resource (a source value)
    :write-fn           a fn from a data representation of the resource
                        (a save-value) to string
    :source-value-fn    a fn from a save-value to whatever you want to cache as
                        the source-value for the resource type. When not
                        specified, the save-value will be the source-value.
    :search-value-fn    a fn from node-id, resource and an evaluation-context to
                        a search-value that can be accepted by the :search-fn in
                        order to find a matching substring inside the resource.
                        If not provided, the nodes :save-value will be used.
    :search-fn          a multi-arity fn used to search the resource for a
                        matching substring. The first arity should accept a
                        search string and return a value that can be used by the
                        second arity to perform the search (typically a regex
                        Pattern). The second arity should accept a search-value
                        returned from the :search-value-fn and the value
                        returned by the first arity, and return a sequence of
                        match-infos. Each match-info should have a :match-type
                        and details about the match unique to that :match-type.
                        This can be used to select the matching sub-region when
                        the match is opened in an editor view.
    :icon               classpath path to an icon image or project resource path
                        string; default \"icons/32/Icons_29-AT-Unknown.png\"
    :icon-class         either :design, :script or :property, controls the
                        resource icon color in UI
    :view-types         vector of alternative views that can be used for
                        resources of the resource type, e.g. :code, :scene,
                        :cljfx-form-view, :text, :html or :default.
    :view-opts          a map from a view-type keyword to options map that will
                        be merged with other opts used when opening a view
    :tags               a set of keywords that can be used for customizing the
                        behavior of the resource throughout the project
    :tag-opts           a map from tag keyword from :tags to additional options
                        map the configures the behavior of the resource with the
                        tag
    :template           classpath or project resource path to a template file
                        for a new resource file creation; defaults to
                        \"templates/template.{ext}\"
    :test-info          a map of type-specific information about the registered
                        resource type that is utilized by the automated tests.
                        Must include a :type field that classifies the method of
                        registration for the tests.
    :label              label for a resource type when shown in the editor,
                        either a string or a MessagePattern instance
    :stateless?         whether or not the node stores any state that needs to
                        be reloaded if the resource is modified externally. When
                        true, we can simply invalidate its outputs without
                        replacing the node in the graph. Defaults to true if
                        there is no :load-fn.
    :lazy-loaded        whether or not we should defer loading of the node until
                        it is opened in the editor. Currently only supported by
                        code editor resource nodes, and only for file types that
                        do not interact with any other nodes in the graph.
    :allow-unloaded-use Allow references to .defunload:ed resources of this type
                        from loaded resources. Basically, this is a declaration
                        that the associated :node-type is well-behaved when
                        referenced despite its :load-fn never have been invoked.
                        This involves producing valid build-targets, scene data,
                        or whatever might be required by the referencing nodes.
    :auto-connect-save-data?    whether changes to the resource are saved
                                to disc (this can also be enabled in load-fn)
                                when there is a :write-fn, default true"
  [workspace & {:keys [textual? language editable ext build-ext node-type load-fn dependencies-fn search-fn search-value-fn source-value-fn read-fn write-fn icon icon-class view-types view-opts tags tag-opts template test-info label stateless? lazy-loaded allow-unloaded-use auto-connect-save-data?]}]
  {:pre [(or (nil? icon-class) (resource/icon-class->style-class icon-class))]}
  (let [editable (if (nil? editable) true (boolean editable))
        textual (true? textual?)
        resource-type {:textual? textual
                       :language (when textual (or language "plaintext"))
                       :editable editable
                       :editor-openable (some? (some editor-openable-view-type? view-types))
                       :node-type node-type
                       :load-fn load-fn
                       :dependencies-fn dependencies-fn
                       :write-fn write-fn
                       :read-fn read-fn
                       :search-fn search-fn
                       :search-value-fn (or search-value-fn default-search-value-fn)
                       :source-value-fn source-value-fn
                       :icon icon
                       :icon-class icon-class
                       :view-types (mapv (partial get-view-type workspace) view-types)
                       :view-opts view-opts
                       :tags tags
                       :tag-opts tag-opts
                       :template template
                       :test-info test-info
                       :label label
                       :stateless? (if (nil? stateless?) (nil? load-fn) stateless?)
                       :lazy-loaded (boolean lazy-loaded)
                       :allow-unloaded-use (boolean allow-unloaded-use)
                       :auto-connect-save-data? (and editable
                                                     (some? write-fn)
                                                     (not (false? auto-connect-save-data?)))}
        resource-types-by-ext (if (string? ext)
                                (let [ext (string/lower-case ext)]
                                  {ext (assoc resource-type :ext ext :build-ext (or build-ext (str ext "c")))})
                                (into {}
                                      (map (fn [ext]
                                             (let [ext (string/lower-case ext)]
                                               (pair ext (assoc resource-type :ext ext :build-ext (or build-ext (str ext "c")))))))
                                      ext))]
    (concat
      (g/update-property workspace :resource-types editable-resource-type-map-update-fn resource-types-by-ext)
      (g/update-property workspace :resource-types-non-editable non-editable-resource-type-map-update-fn resource-types-by-ext))))

(defn get-resource-type-map
  ([workspace]
   (resource/resource-types-by-type-ext (g/unsafe-basis) workspace :editable))
  ([workspace editability]
   (resource/resource-types-by-type-ext (g/unsafe-basis) workspace editability)))

(defn get-resource-type
  ([workspace ext]
   (get (get-resource-type-map workspace) ext))
  ([workspace editability ext]
   (get (get-resource-type-map workspace editability) ext)))

(defn make-memory-resource [workspace editability ext data]
  (let [resource-type-map (get-resource-type-map workspace editability)]
    (if-some [resource-type (resource-type-map ext)]
      (resource/make-memory-resource workspace resource-type data)
      (throw (ex-info (format "Unable to locate resource type info. Extension not loaded? (type=%s)"
                              ext)
                      {:type ext
                       :registered-types (into (sorted-set)
                                               (keys resource-type-map))})))))

(defn make-placeholder-resource
  ([workspace ext]
   (make-memory-resource workspace :editable ext nil))
  ([workspace editability ext]
   (make-memory-resource workspace editability ext nil)))

(defn make-placeholder-build-resource
  ([workspace ext]
   (make-build-resource (make-placeholder-resource workspace :editable ext)))
  ([workspace editability ext]
   (make-build-resource (make-placeholder-resource workspace editability ext))))

(defn resource-icon [resource]
  (when resource
    (if (and (resource/read-only? resource)
             (= (resource/path resource) (resource/resource-name resource)))
      "icons/32/Icons_03-Builtins.png"
      (condp = (resource/source-type resource)
        :file
        (or (:icon (resource/resource-type resource)) "icons/32/Icons_29-AT-Unknown.png")
        :folder
        "icons/32/Icons_01-Folder-closed.png"))))

(defn file-resource
  ([workspace path-or-file]
   (file-resource (g/now) workspace path-or-file))
  ([basis workspace path-or-file]
   (let [workspace-node (g/node-by-id basis workspace)
         project-path (g/raw-property-value* basis workspace-node :root)
         editable-proj-path? (g/raw-property-value* basis workspace-node :editable-proj-path?)
         unloaded-proj-path? (g/raw-property-value* basis workspace-node :unloaded-proj-path?)
         file (if (instance? File path-or-file)
                path-or-file
                (File. (str project-path path-or-file)))]
     (resource/make-file-resource workspace project-path file [] editable-proj-path? unloaded-proj-path?))))

(defn find-resource
  ([workspace proj-path]
   (g/with-auto-evaluation-context evaluation-context
     (find-resource workspace proj-path evaluation-context)))
  ([workspace proj-path evaluation-context]
   ;; This is frequently called from property setters, where we don't have a
   ;; cache. In that case, manually cache the evaluated value in the
   ;; :tx-data-context atom of the evaluation-context, since this persists
   ;; throughout the transaction.
   (let [resources-by-proj-path (g/tx-cached-node-value! workspace :resource-map evaluation-context)]
     (get resources-by-proj-path proj-path))))

(defn resolve-workspace-resource
  ([workspace path]
   (when (not-empty path)
     (g/with-auto-evaluation-context evaluation-context
       (or
         (find-resource workspace path evaluation-context)
         (file-resource (:basis evaluation-context) workspace path)))))
  ([workspace path evaluation-context]
   (when (not-empty path)
     (or
       (find-resource workspace path evaluation-context)
       (file-resource (:basis evaluation-context) workspace path)))))

(defn make-proj-path->resource-fn [workspace evaluation-context]
  (let [basis (:basis evaluation-context)
        workspace-node (g/node-by-id basis workspace)
        project-path (g/raw-property-value* basis workspace-node :root)
        editable-proj-path? (g/raw-property-value* basis workspace-node :editable-proj-path?)
        unloaded-proj-path? (g/raw-property-value* basis workspace-node :unloaded-proj-path?)
        resources-by-proj-path (g/tx-cached-node-value! workspace :resource-map evaluation-context)

        make-missing-file-resource
        (fn/memoize
          (fn make-missing-file-resource [proj-path]
            (let [file (io/file (str project-path proj-path))]
              (resource/make-file-resource workspace project-path file [] editable-proj-path? unloaded-proj-path?))))]

    (assert (not (g/error? resources-by-proj-path)))
    (assert (map? resources-by-proj-path))
    (fn proj-path->resource [proj-path]
      (when-not (coll/empty? proj-path)
        (or (resources-by-proj-path proj-path)
            (make-missing-file-resource proj-path))))))

(defn- absolute-path [^String path]
  (.startsWith path "/"))

(defn to-absolute-path
  ([rel-path] (to-absolute-path "" rel-path))
  ([base rel-path]
   (if (absolute-path rel-path)
     rel-path
     (str base "/" rel-path))))

(defn resolve-resource
  ([base-resource path]
   (g/with-auto-evaluation-context evaluation-context
     (resolve-resource base-resource path evaluation-context)))
  ([base-resource path evaluation-context]
   (when-not (empty? path)
     (let [basis (:basis evaluation-context)
           workspace (:workspace base-resource)
           path  (if (absolute-path path)
                   path
                   (resource/file->proj-path (project-directory basis workspace)
                                             (.getCanonicalFile (io/file (.getParentFile (io/file base-resource))
                                                                         path))))]
       (resolve-workspace-resource workspace path evaluation-context)))))

(def ^:private default-user-resource-path "/templates/default.")
(def ^:private java-resource-path "templates/template.")

(defn- get-template-resource [workspace resource-type evaluation-context]
  (when resource-type
    (let [resource-path (:template resource-type)
          ext (:ext resource-type)]
      (or
        ;; default user resource
        (find-resource workspace (str default-user-resource-path ext) evaluation-context)
        ;; editor resource provided from extensions
        (when resource-path (find-resource workspace resource-path evaluation-context))
        ;; java resource
        (io/resource (str java-resource-path ext))))))

(defn has-template?
  ([workspace resource-type]
   (g/with-auto-evaluation-context evaluation-context
     (has-template? workspace resource-type evaluation-context)))
  ([workspace resource-type evaluation-context]
   (some? (get-template-resource workspace resource-type evaluation-context))))

(defn template
  ([workspace resource-type]
   (g/with-auto-evaluation-context evaluation-context
     (template workspace resource-type evaluation-context)))
  ([workspace resource-type evaluation-context]
   (when-let [resource (get-template-resource workspace resource-type evaluation-context)]
     (let [{:keys [read-fn write-fn]} resource-type]
       (if (and read-fn write-fn)
         ;; Sanitize the template.
         (write-fn
           (with-open [reader (io/reader resource)]
             (read-fn reader)))

         ;; Just read the file as-is.
         (with-open [reader (io/reader resource)]
           (slurp reader)))))))

(defn replace-template-name
  ^String [^String template ^String name]
  (let [escaped-name (protobuf/escape-string name)]
    (string/replace template "{{NAME}}" escaped-name)))

(defn- update-dependency-notifications! [workspace lib-states]
  (let [{:keys [error missing]} (->> lib-states
                                     (eduction
                                       (keep (fn [{:keys [status file uri]}]
                                               (cond
                                                 (= status :error) (pair :error uri)
                                                 (nil? file) (pair :missing uri)))))
                                     (iutil/group-into {} [] key val))
        notifications (notifications workspace)
        min-version-states (into [] (filter #(= :defold-min-version (:reason %))) lib-states)
        failing-uris (into #{} (map :uri) min-version-states)]
    ;; Single min-version notification (only first error).
    (if-let [{:keys [uri required current]} (first min-version-states)]
      (notifications/show!
        notifications
        {:id ::dependencies-min-version
         :type :error
         :message (localization/message
                    "notification.fetch-libraries.min-version.error"
                    {"uri" (str uri)
                     "required" (str required)
                     "current" (str current)})
         :actions [{:message (localization/message "notification.fetch-libraries.dependencies-error.action.open-game-project")
                    :on-action #(ui/execute-command
                                  (ui/contexts (ui/main-scene) true)
                                  :file.open
                                  "/game.project")}]})
      (notifications/close! notifications ::dependencies-min-version))
    (if (pos? (count missing))
      (notifications/show!
        notifications
        {:id ::dependencies-missing
         :type :warning
         :message (localization/message
                    "notification.fetch-libraries.dependencies-missing.warning"
                    {"dependencies" (coll/join-to-string "\n" (e/map dialogs/indent-with-bullet missing))})
         :actions [{:message (localization/message "notification.fetch-libraries.dependencies-changed.action.fetch")
                    :on-action #(ui/execute-command
                                  (ui/contexts (ui/main-scene) true)
                                  :project.fetch-libraries
                                  nil)}]})
      (notifications/close! notifications ::dependencies-missing))
    (let [other-errors (into [] (remove failing-uris) (or error []))]
      (if (pos? (count other-errors))
        (notifications/show!
          notifications
          {:id ::dependencies-error
           :type :error
           :message (localization/message
                      "notification.fetch-libraries.dependencies-error.error"
                      {"dependencies" (coll/join-to-string "\n" (e/map dialogs/indent-with-bullet other-errors))})
           :actions [{:message (localization/message "notification.fetch-libraries.dependencies-error.action.open-game-project")
                      :on-action #(ui/execute-command
                                    (ui/contexts (ui/main-scene) true)
                                    :file.open
                                    "/game.project")}]})
        (notifications/close! notifications ::dependencies-error)))))

(defn set-project-dependencies! [workspace lib-states]
  (g/set-property! workspace :dependencies lib-states)
  (update-dependency-notifications! workspace lib-states)
  lib-states)

(defn dependencies
  ([workspace]
   (g/with-auto-evaluation-context evaluation-context
     (dependencies workspace evaluation-context)))
  ([workspace evaluation-context]
   (g/node-value workspace :dependency-uris evaluation-context)))

(defn dependencies-reachable? [dependencies]
  (let [hosts (into #{} (map url/strip-path) dependencies)]
    (every? url/reachable? hosts)))

(defn make-snapshot-info [workspace project-path dependencies snapshot-cache]
  (let [snapshot-info (resource-watch/make-snapshot-info workspace project-path dependencies snapshot-cache)]
    (assoc snapshot-info :map (resource-watch/make-resource-map (:snapshot snapshot-info)))))

(defn update-snapshot-cache! [workspace snapshot-cache]
  (g/set-property! workspace :snapshot-cache snapshot-cache))

(defn snapshot-cache [workspace]
  (g/node-value workspace :snapshot-cache))

(defn- is-plugin-clojure-file? [resource]
  (= "clj" (resource/ext resource)))

(defn- load-clojure-plugin! [workspace resource]
  (when-not (Boolean/getBoolean "defold.tests")
    (log/info :message (str "Loading plugin " (resource/path resource))))
  (try
    (if-let [plugin-fn (load-string (slurp resource))]
      (do
        (plugin-fn workspace)
        (when-not (Boolean/getBoolean "defold.tests")
          (log/info :message (str "Loaded plugin " (resource/path resource)))))
      (log/error :message (str "Unable to load plugin " (resource/path resource))))
    (catch Exception e
      (log/error :message (str "Exception while loading plugin: " (.getMessage e))
                 :exception e)
      (ui/run-later
        (dialogs/make-info-dialog
          (g/with-auto-evaluation-context evaluation-context
            (localization workspace evaluation-context))
          {:title (localization/message "dialog.plugin-load-error.title")
           :icon :icon/triangle-error
           :always-on-top true
           :header (localization/message "dialog.plugin-load-error.header" {"plugin" (resource/proj-path resource)})}))
      false)))

(defn load-clojure-editor-plugins! [workspace added]
  (->> added
       (filterv is-plugin-clojure-file?)
       ;; FIXME: hack for extension-spine: spineguiext.clj requires spineext.clj
       ;;        that needs to be loaded first
       (sort-by resource/proj-path util/natural-order)
       (run! #(load-clojure-plugin! workspace %))))

; Determine if the extension has plugins, if so, it needs to be extracted

(defn- jar-file? [resource]
  (= "jar" (resource/ext resource)))

; from native_extensions.clj
(defn- extension-root?
  [resource]
  (some #(= "ext.manifest" (resource/resource-name %)) (resource/children resource)))

(defn- find-parent [resource evaluation-context]
  (let [parent-path (resource/parent-proj-path (resource/proj-path resource))]
    (find-resource (resource/workspace resource) (str parent-path) evaluation-context)))

(defn- is-extension-file? [resource evaluation-context]
  (or (extension-root? resource)
      (some-> (find-parent resource evaluation-context)
              (recur evaluation-context))))

(defn- is-plugin-file? [resource evaluation-context]
  (and
    (= :file (resource/source-type resource))
    (string/includes? (resource/proj-path resource) "/plugins/")
    (is-extension-file? resource evaluation-context)))

(defn- shared-library? [resource]
  (contains? #{"dylib" "dll" "so"} (resource/ext resource)))

(defn- add-to-path-property [propertyname path]
  (let [current (System/getProperty propertyname)
        newvalue (if current
                   (str current File/pathSeparator path)
                   path)]
    (System/setProperty propertyname newvalue)))

(defn- register-jar-file! [jar-file]
  (.addURL ^DynamicClassLoader java/class-loader (io/as-url jar-file)))

(defn- native-library-parent-dir-allowed? [parent-dir-name]
    (->> (Platform/getHostPlatform)
         .getExtenderPaths
         (some #(= parent-dir-name %))
         boolean))

(defn- register-shared-library-file! [^File shared-library-file]
  (let [parent-dir-file (.getParentFile shared-library-file)]
    (when (native-library-parent-dir-allowed? (.getName parent-dir-file))
      (let [parent-dir (str parent-dir-file)]
        (add-to-path-property "jna.library.path" parent-dir)
        (add-to-path-property "java.library.path" parent-dir)))))

(defn unpack-resource!
  ([workspace resource]
   (unpack-resource! workspace nil resource))
  ([workspace infix-path resource]
   (try
     (let [resource-path (str infix-path (resource/proj-path resource))
           target-file (plugin-path workspace resource-path)]
       (fs/create-parent-directories! target-file)
       (with-open [is (io/input-stream resource)]
         (io/copy is target-file))
       (when (string/includes? resource-path "/plugins/bin/")
         (.setExecutable target-file true))
       (when (jar-file? resource)
         (register-jar-file! target-file))
       (when (shared-library? resource)
         (register-shared-library-file! target-file)))
     (catch FileNotFoundException e
       (throw (IOException. "\nExtension plugins needs updating.\nPlease restart editor for these changes to take effect!" e))))))

(defn- delete-directory-recursive [^File file]
  ;; Recursively delete a directory. // https://gist.github.com/olieidel/c551a911a4798312e4ef42a584677397
  ;; when `file` is a directory, list its entries and call this
  ;; function with each entry. can't `recur` here as it's not a tail
  ;; position, sadly. could cause a stack overflow for many entries?
  ;; thanks to @nikolavojicic for the idea to use `run!` instead of
  ;; `doseq` :)
  (when (.isDirectory file)
    (run! delete-directory-recursive (.listFiles file)))
  ;; delete the file or directory. if it's a file, it's easily
  ;; deletable. if it's a directory, we already have deleted all its
  ;; contents with the code above (remember?)
  (io/delete-file file))

(defn clean-editor-plugins! [workspace]
  ; At startup, we want to remove the plugins in order to avoid having issues copying the .dll on Windows
  (let [dir (plugin-path workspace)]
    (if (.exists dir)
      (delete-directory-recursive dir))))

(def ^:private plugin-zip-names
  (let [platform (Platform/getHostPlatform)]
    {"/plugins/common.zip" 0
     (str "/plugins/" (.getOs platform) ".zip") 1
     (str "/plugins/" (.getPair platform) ".zip") 2}))

(defn- plugin-zip-priority [resource]
  (plugin-zip-names (re-find #"/plugins/[^/]+\.zip" (resource/proj-path resource))))

(defn- plugin-zip? [resource]
  (let [path (resource/proj-path resource)]
    (some? (some #(string/ends-with? path %) (keys plugin-zip-names)))))

(defn- unpack-plugin-zip! [workspace resource]
  {:pre [(string/ends-with? (resource/proj-path resource) ".zip")]}
  (unpack-resource! workspace resource)
  (let [proj-path (resource/proj-path resource)
        plugin-file (plugin-path workspace proj-path)
        infix-path (resource/parent-proj-path proj-path)]
    (run! #(unpack-resource! workspace infix-path %)
          (eduction
            (mapcat resource/resource-seq)
            (filter #(= :file (resource/source-type %)))
            (:tree (resource/load-zip-resources workspace plugin-file))))))

(defn unpack-editor-plugins! [workspace changed]
  ; Used for unpacking the .jar files and shared libraries (.so, .dylib, .dll) to disc
  ; TODO: Handle removed plugins (e.g. a dependency was removed)
  (g/let-ec [[plugin-zips resources]
             (->> changed
                  (filter #(is-plugin-file? % evaluation-context))
                  (coll/separate-by plugin-zip?))

             plugin-zips
             (->> plugin-zips
                  (into []
                        (comp
                          (map #(find-parent % evaluation-context))
                          (distinct)
                          (mapcat resource/children)
                          (filter plugin-zip?)))
                  (sort-by plugin-zip-priority))]
    (run! #(unpack-plugin-zip! workspace %) plugin-zips)
    (run! #(unpack-resource! workspace %) resources)))

(defn- sync-snapshot-errors-notifications! [workspace old-errors new-errors]
  (when (or old-errors new-errors)
    (letfn [(errors->collision-resource-path+statuses [errors]
              (into #{}
                    (comp
                      (filter #(= :collision (:type %)))
                      (mapcat :collisions)
                      (filter #(re-matches #"^/[^/]*$" (key %))))
                    errors))
            (collision-notification-id [resource-path]
              [::resource-collision-notification resource-path])]
      (let [old-collisions (errors->collision-resource-path+statuses old-errors)
            new-collisions (errors->collision-resource-path+statuses new-errors)
            removed (set/difference old-collisions new-collisions)
            added (set/difference new-collisions old-collisions)
            notifications-node (notifications workspace)]
        (doseq [[resource-path] removed]
          (notifications/close! notifications-node (collision-notification-id resource-path)))
        (doseq [[resource-path {:keys [source] :as status}] (sort-by key added)]
          (notifications/show!
            notifications-node
            {:type :warning
             :id (collision-notification-id resource-path)
             :message (localization/message
                        (case source
                          :library "notification.resource-collision.library.warning"
                          :builtins "notification.resource-collision.builtins.warning"
                          "notification.resource-collision.directory.warning")
                        {"resource" resource-path
                         "library" (:library status)})}))))))

(defn resource-sync!
  ([workspace]
   (resource-sync! workspace []))
  ([workspace moved-files]
   (resource-sync! workspace moved-files progress/null-render-progress!))
  ([workspace moved-files render-progress!]
   (let [snapshot-info (make-snapshot-info workspace (project-directory workspace) (dependencies workspace) (snapshot-cache workspace))
         {new-snapshot :snapshot new-map :map new-snapshot-cache :snapshot-cache} snapshot-info]
     (update-snapshot-cache! workspace new-snapshot-cache)
     (resource-sync! workspace moved-files render-progress! new-snapshot new-map)))
  ([workspace moved-files render-progress! new-snapshot new-map]
   (let [project-directory (project-directory workspace)
         moved-proj-paths (keep (fn [[src tgt]]
                                  (let [src-path (resource/file->proj-path project-directory src)
                                        tgt-path (resource/file->proj-path project-directory tgt)]
                                    (assert (some? src-path) (str "project does not contain source " (pr-str src)))
                                    (assert (some? tgt-path) (str "project does not contain target " (pr-str tgt)))
                                    (when (not= src-path tgt-path)
                                      [src-path tgt-path])))
                                moved-files)
         old-snapshot (g/node-value workspace :resource-snapshot)
         old-map      (resource-watch/make-resource-map old-snapshot)
         changes      (resource-watch/diff old-snapshot new-snapshot)]
     (sync-snapshot-errors-notifications! workspace (:errors old-snapshot) (:errors new-snapshot))
     (when (or (not (resource-watch/empty-diff? changes)) (seq moved-proj-paths))
       (g/set-property! workspace :resource-snapshot new-snapshot)
       (let [changes (into {} (map (fn [[type resources]] [type (filter #(= :file (resource/source-type %)) resources)]) changes))
             move-source-paths (map first moved-proj-paths)
             move-target-paths (map second moved-proj-paths)
             chain-moved-paths (set/intersection (set move-source-paths) (set move-target-paths))
             merged-target-paths (set (map first (filter (fn [[k v]] (> v 1)) (frequencies move-target-paths))))
             moved (keep (fn [[source-path target-path]]
                           (when-not (or
                                       ;; resource sync currently can't handle chained moves, so refactoring is
                                       ;; temporarily disabled for those cases (no move pair)
                                       (chain-moved-paths source-path) (chain-moved-paths target-path)
                                       ;; also can't handle merged targets, multiple files with same name moved to same dir
                                       (merged-target-paths target-path))
                             (let [src-resource (old-map source-path)
                                   tgt-resource (new-map target-path)]
                               ;; We used to (assert (some? src-resource)), but this could fail for instance if
                               ;; * source-path refers to a .dotfile (like .DS_Store) that we ignore in resource-watch
                               ;; * Some external process has created a file in a to-be-moved directory and we haven't run a resource-sync! before the move
                               ;; We handle these cases by ignoring the move. Any .dotfiles will stay ignored, and any new files will pop up as :added
                               ;;
                               ;; We also used to (assert (some? tgt-resource)) but an arguably very unlikely case is that the target of the move is
                               ;; deleted from disk after the move but before the snapshot.
                               ;; We handle that by ignoring the move and effectively treating target as just :removed.
                               ;; The source will be :removed or :changed (if a library snuck in).
                               (cond
                                 (nil? src-resource)
                                 (do (log/warn :msg (str "can't find source of move " source-path)) nil)

                                 (nil? tgt-resource)
                                 (do (log/warn :msg (str "can't find target of move " target-path)) nil)

                                 (and (= :file (resource/source-type src-resource))
                                      (= :file (resource/source-type tgt-resource))) ; paranoia
                                 [src-resource tgt-resource]))))
                         moved-proj-paths)
             changes-with-moved (assoc changes :moved moved)]
         (assert (= (count (distinct (map (comp resource/proj-path first) moved)))
                    (count (distinct (map (comp resource/proj-path second) moved)))
                    (count moved))) ; no overlapping sources, dito targets
         (assert (= (count (distinct (concat (map (comp resource/proj-path first) moved)
                                             (map (comp resource/proj-path second) moved))))
                    (* 2 (count moved)))) ; no chained moves src->tgt->tgt2...
         (assert (empty? (set/intersection (set (map (comp resource/proj-path first) moved))
                                           (set (map resource/proj-path (:added changes)))))) ; no move-source is in :added
         (try
           (let [listeners @(g/node-value workspace :resource-listeners)
                 total-progress-size (transduce (map first) + 0 listeners)]
             (loop [listeners listeners
                    parent-progress (progress/make localization/empty-message total-progress-size)]
               (when-some [[progress-span listener] (first listeners)]
                 (resource/handle-changes listener changes-with-moved
                                          (progress/nest-render-progress render-progress! parent-progress progress-span))
                 (recur (next listeners)
                        (progress/advance parent-progress progress-span)))))
           (finally
             (render-progress! progress/done)))))
     changes)))

(defn fetch-and-validate-libraries [workspace library-uris render-fn]
  (->> (library/current-library-state (project-directory workspace) library-uris)
       (library/fetch-library-updates library/default-http-resolver render-fn)
       (library/validate-updated-libraries)))

(defn install-validated-libraries! [workspace lib-states]
  (let [new-lib-states (library/install-validated-libraries! (project-directory workspace) lib-states)]
    (set-project-dependencies! workspace new-lib-states)))

(defn add-resource-listener! [workspace progress-span listener]
  (swap! (g/node-value workspace :resource-listeners) conj [progress-span listener]))

(defn prepend-resource-listener! [workspace progress-span listener]
  ;; Used from tests that need to interrogate the resource change plan before it
  ;; is executed.
  (swap! (g/node-value workspace :resource-listeners)
         (fn [resource-listener-entries]
           (let [resource-listener-entry [progress-span listener]]
             (into [resource-listener-entry]
                   resource-listener-entries)))))

(g/deftype Dependencies
  [{:uri URI
    (s/optional-key :file) File
    s/Keyword s/Any}])

(g/defnode Workspace
  (property root g/Str)
  (property dependencies Dependencies)
  (property opened-files g/Any)
  (property resource-snapshot g/Any (default resource-watch/empty-snapshot))
  (property resource-listeners g/Any)
  (property disk-sha256s-by-node-id g/Any (default {}))
  (property view-types g/Any (default {:default {:id :default}}))
  (property resource-types g/Any)
  (property resource-types-non-editable g/Any)
  (property snapshot-cache g/Any (default {}))
  (property build-settings g/Any)
  (property editable-proj-path? g/Any)
  (property unloaded-proj-path? g/Any)
  (property resource-kind-extensions g/Any (default {:atlas ["atlas" "tilesource"]}))
  (property node-attachments g/Any (default {}))
  (property localization g/Any)

  (input code-preprocessors g/NodeID :cascade-delete)
  (input notifications g/NodeID :cascade-delete)

  (output dependency-uris g/Any (g/fnk [dependencies] (mapv :uri dependencies)))
  (output dependencies g/Any (g/fnk [dependencies] dependencies))
  (output resource-tree FileResource :cached produce-resource-tree)
  (output resource-list g/Any :cached produce-resource-list)
  (output resource-map g/Any :cached produce-resource-map))

(defn node-attachments [basis workspace]
  (g/raw-property-value basis workspace :node-attachments))

;; SDK api
(defn register-resource-kind-extension [workspace resource-kind extension]
  (g/update-property
    workspace :resource-kind-extensions
    (fn [extensions-by-resource-kind]
      (if-some [extensions (extensions-by-resource-kind resource-kind)]
        (if (neg? (coll/index-of extensions extension))
          (assoc extensions-by-resource-kind resource-kind (conj extensions extension))
          extensions-by-resource-kind) ; Already registered, return unaltered.
        (throw (IllegalArgumentException. (str "Unsupported resource-kind:" resource-kind)))))))

(defn resource-kind-extensions [workspace resource-kind evaluation-context]
  ;; TODO: This is often abused inside production functions, but this data
  ;; should really be passed through graph connections.
  (let [extensions-by-resource-kind (g/node-value workspace :resource-kind-extensions evaluation-context)]
    (or (extensions-by-resource-kind resource-kind)
        (throw (IllegalArgumentException. (str "Unsupported resource-kind:" resource-kind))))))

(defn make-build-settings
  [prefs]
  {:compress-textures? (prefs/get prefs [:build :texture-compression])})

(defn update-build-settings!
  [workspace prefs]
  (g/set-property! workspace :build-settings (make-build-settings prefs)))

(defn artifact-map [workspace]
  (g/user-data workspace ::artifact-map))

(defn artifact-map! [workspace artifact-map]
  (g/user-data! workspace ::artifact-map artifact-map))

(defn etags [workspace]
  (g/user-data workspace ::etags))

(defn etag [workspace proj-path]
  (get (etags workspace) proj-path))

(defn etags! [workspace etags]
  (g/user-data! workspace ::etags etags))

(defn- artifact-map-file
  ^File [workspace]
  (io/file (build-path workspace) ".artifact-map"))

(defn- try-read-artifact-map [^File file]
  (when (.exists file)
    (try
      (with-open [reader (PushbackReader. (io/reader file))]
        (edn/read reader))
      (catch Exception error
        (log/warn :msg "Failed to read artifact map. Build cache invalidated." :exception error)
        nil))))

(defn artifact-map->etags [artifact-map]
  (when (seq artifact-map)
    (into {}
          (map (juxt key (comp :etag val)))
          artifact-map)))

(defn load-build-cache! [workspace]
  (let [file (artifact-map-file workspace)
        artifact-map (try-read-artifact-map file)
        etags (artifact-map->etags artifact-map)]
    (artifact-map! workspace artifact-map)
    (etags! workspace etags)
    nil))

(defn save-build-cache! [workspace]
  (let [file (artifact-map-file workspace)
        artifact-map (artifact-map workspace)]
    (if (empty? artifact-map)
      (fs/delete-file! file)
      (let [saved-artifact-map (into (sorted-map) artifact-map)]
        (fs/create-file! file (pr-str saved-artifact-map))))
    nil))

(defn clear-build-cache! [workspace]
  (let [file (artifact-map-file workspace)]
    (artifact-map! workspace nil)
    (etags! workspace nil)
    (fs/delete-file! file)
    nil))

(defn- make-editable-proj-path-predicate [non-editable-directory-proj-paths]
  {:pre [(vector? non-editable-directory-proj-paths)
         (every? string? non-editable-directory-proj-paths)]}
  (fn editable-proj-path? [proj-path]
    (not-any? (fn [non-editable-directory-proj-path]
                ;; A proj-path is considered non-editable if it matches or is
                ;; located below a non-editable directory. Thus, the character
                ;; immediately following the non-editable directory should be a
                ;; slash, or should be the end of the proj-path string. We can
                ;; test this fact before matching the non-editable directory
                ;; path against the beginning of the proj-path to make the test
                ;; more efficient.
                (case (get proj-path (count non-editable-directory-proj-path))
                  (\/ nil) (string/starts-with? proj-path non-editable-directory-proj-path)
                  false))
              non-editable-directory-proj-paths)))

(defn has-non-editable-directories?
  ([workspace]
   (has-non-editable-directories? (g/now) workspace))
  ([basis workspace]
   (not= fn/constantly-true
         (g/raw-property-value basis workspace :editable-proj-path?))))

(defn make-workspace [graph project-path build-settings workspace-config localization]
  (let [project-directory (.getCanonicalFile (io/file project-path))
        unloaded-proj-path? (resource/defunload-pred project-directory)
        editable-proj-path? (if-some [non-editable-directory-proj-paths (not-empty (:non-editable-directories workspace-config))]
                              (make-editable-proj-path-predicate non-editable-directory-proj-paths)
                              fn/constantly-true)]
    (first
      (g/tx-nodes-added
        (g/transact
          (g/make-nodes graph
            [workspace [Workspace
                        :root (.getPath project-directory)
                        :opened-files (atom #{})
                        :resource-listeners (atom [])
                        :build-settings build-settings
                        :editable-proj-path? editable-proj-path?
                        :unloaded-proj-path? unloaded-proj-path?
                        :localization localization]
             code-preprocessors code.preprocessors/CodePreprocessorsNode
             notifications notifications/NotificationsNode]
            (concat
              (g/connect notifications :_node-id workspace :notifications)
              (g/connect code-preprocessors :_node-id workspace :code-preprocessors))))))))

(defn set-disk-sha256 [workspace node-id disk-sha256]
  {:pre [(g/node-id? workspace)
         (g/node-id? node-id)
         (or (nil? disk-sha256) (digest/sha256-hex? disk-sha256))]}
  (g/update-property workspace :disk-sha256s-by-node-id assoc node-id disk-sha256))

(defn merge-disk-sha256s [workspace disk-sha256s-by-node-id]
  {:pre [(g/node-id? workspace)
         (map? disk-sha256s-by-node-id)
         (every? g/node-id? (keys disk-sha256s-by-node-id))
         (every? #(or (nil? %) (digest/sha256-hex? %)) (vals disk-sha256s-by-node-id))]}
  (when-not (coll/empty? disk-sha256s-by-node-id)
    (g/update-property workspace :disk-sha256s-by-node-id into disk-sha256s-by-node-id)))

(defn register-view-type
  "Register a new view type that can be used by resources

  Required kv-args:
    :id       keyword identifying the view type
    :label    a label for the view type shown in the editor, localization
              MessagePattern or a string

  Optional kv-args:
    :make-view-fn          fn of graph, parent (AnchorPane), resource node and
                           opts that should create new view node, set it up and
                           return the node id; opts is a map that will contain:
                           - :app-view
                           - :select-fn
                           - :prefs
                           - :project
                           - :workspace
                           - :tab (Tab instance)
                           - all opts from resource-type's :view-opts
                           - any extra opts passed from the code
                           if not present, the resource will be opened in
                           the OS-associated application
    :make-preview-fn       fn of graph, resource node, opts, width and height
                           that should return a node id with :image output (with
                           value of type Image); opts is a map with:
                           - :app-view
                           - :select-fn
                           - :project
                           - :workspace
                           - all opts from resource-type's :view-opts
                           This preview will be used in select resource dialog
                           on hover over resources
    :dispose-preview-fn    fn of node id returned by :make-preview-fn, will be
                           invoked on preview dispose
    :focus-fn              fn of node id returned by :make-view-fn and opts,
                           will be called on resource open request, opts will
                           only contain data passed from the code (e.g.
                           :cursor-range)
    :text-selection-fn     fn of node id returned by :make-view-fn, should
                           return selected text as a string or nil; will be used
                           to pre-populate Open Assets and Search in Files
                           dialogs"
  [workspace & {:keys [id label make-view-fn make-preview-fn dispose-preview-fn focus-fn text-selection-fn]}]
  (let [view-type (merge {:id    id
                          :label label}
                         (when make-view-fn
                           {:make-view-fn make-view-fn})
                         (when make-preview-fn
                           {:make-preview-fn make-preview-fn})
                         (when dispose-preview-fn
                           {:dispose-preview-fn dispose-preview-fn})
                         (when focus-fn
                           {:focus-fn focus-fn})
                         (when text-selection-fn
                           {:text-selection-fn text-selection-fn}))]
     (g/update-property workspace :view-types assoc (:id view-type) view-type)))
