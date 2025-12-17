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

(ns integration.test-util
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [clojure.test :as test :refer [is testing]]
            [clojure.test.check.clojure-test]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.atlas :as atlas]
            [editor.build :as build]
            [editor.code.data :as code.data]
            [editor.collection :as collection]
            [editor.defold-project :as project]
            [editor.disk :as disk]
            [editor.editor-extensions :as extensions]
            [editor.field-expression :as field-expression]
            [editor.fs :as fs]
            [editor.game-object :as game-object]
            [editor.graph-util :as gu]
            [editor.handler :as handler]
            [editor.localization :as localization]
            [editor.material :as material]
            [editor.math :as math]
            [editor.outline :as outline]
            [editor.particlefx :as particlefx]
            [editor.prefs :as prefs]
            [editor.progress :as progress]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.resource-types :as resource-types]
            [editor.scene :as scene]
            [editor.scene-selection :as scene-selection]
            [editor.scene-tools :as scene-tools]
            [editor.settings :as settings]
            [editor.settings-core :as settings-core]
            [editor.shared-editor-settings :as shared-editor-settings]
            [editor.tile-map :as tile-map]
            [editor.ui :as ui]
            [editor.view :as view]
            [editor.workspace :as workspace]
            [internal.system :as is]
            [internal.util :as util]
            [lambdaisland.deep-diff2 :as deep-diff]
            [service.log :as log]
            [support.test-support :as test-support]
            [util.coll :refer [pair]]
            [util.diff :as diff]
            [util.fn :as fn]
            [util.http-server :as http-server]
            [util.text-util :as text-util]
            [util.thread-util :as thread-util])
  (:import [ch.qos.logback.classic Level Logger]
           [clojure.core Vec]
           [com.google.protobuf ByteString]
           [editor.properties Curve CurveSpread]
           [java.awt.image BufferedImage]
           [java.io ByteArrayOutputStream File FileInputStream FilenameFilter]
           [java.lang AutoCloseable]
           [java.net URI URL]
           [java.nio.file Files]
           [java.util UUID]
           [java.util.concurrent LinkedBlockingQueue]
           [java.util.zip ZipEntry ZipOutputStream]
           [javafx.event ActionEvent]
           [javafx.scene Parent Scene]
           [javafx.scene.control Button Cell ColorPicker Control Label ScrollBar Slider TextField ToggleButton]
           [javafx.scene.input KeyCode KeyEvent]
           [javafx.scene.layout VBox]
           [javafx.scene.paint Color]
           [javax.imageio ImageIO]
           [javax.vecmath Vector3d]
           [org.apache.commons.io FilenameUtils IOUtils]
           [org.slf4j LoggerFactory]))

(set! *warn-on-reflection* true)

(.setLevel ^Logger (LoggerFactory/getLogger "org.eclipse.jetty") Level/ERROR)
(.setLevel ^Logger (LoggerFactory/getLogger "cognitect.aws.credentials") Level/ERROR)

;; Disable defspec logs:
;; {:result true, :num-tests 100, :seed 1761047757693, :time-elapsed-ms 41, :test-var "some-spec"}
(alter-var-root #'clojure.test.check.clojure-test/*report-completion* (constantly false))

(def project-path "test/resources/test_project")

(def ^:private ^:const system-cache-size 1000)

;; String urls that will be added as library dependencies to our test project.
;; These extensions register additional protobuf resource types that we want to
;; cover in our tests.
(def sanctioned-extension-urls
  (mapv #(System/getProperty %)
        ["defold.extension.rive.url"
         "defold.extension.simpledata.url"
         "defold.extension.spine.url"
         "defold.extension.texturepacker.url"]))

(defn number-type-preserving? [a b]
  (assert (or (number? a) (vector? a) (instance? Curve a) (instance? CurveSpread a)))
  (assert (or (number? b) (vector? b) (instance? Curve b) (instance? CurveSpread b)))
  (and (is (= (type a) (type b)))
       (or (number? a)
           (and (is (identical? (meta a) (meta b)))
                (cond
                  (vector? a)
                  (and (or (not (instance? Vec a))
                           (is (= (type (.am ^Vec a))
                                  (type (.am ^Vec b)))))
                       (testing "Vector elements"
                         (every? true? (map number-type-preserving? a (cycle b)))
                         (every? true? (map number-type-preserving? b (cycle a)))))

                  (instance? Curve a)
                  (testing "Curve points"
                    (true? (number-type-preserving? (properties/curve-vals a) (properties/curve-vals b))))

                  (instance? CurveSpread a)
                  (and (testing "CurveSpread spread"
                         (true? (number-type-preserving? (:spread a) (:spread b))))
                       (testing "CurveSpread points"
                         (true? (number-type-preserving? (properties/curve-vals a) (properties/curve-vals b))))))))))

(def ensure-number-type-preserving! number-type-preserving?)

(defn editable-controls [^Parent parent]
  (->> parent
       (tree-seq #(instance? Parent %)
                 #(.getChildrenUnmodifiable ^Parent %))
       (filterv #(and (instance? Control %)
                      (not (or (instance? Cell %)
                               (instance? Label %)
                               (instance? Button %)
                               (instance? ScrollBar %)))))))

(defmulti set-control-value! (fn [^Control control _num-value] (class control)))

(defmethod set-control-value! ColorPicker [^ColorPicker color-picker num-value]
  (.setValue color-picker (Color/gray num-value))
  (.fireEvent color-picker (ActionEvent. color-picker color-picker)))

(defmethod set-control-value! Slider [^Slider slider num-value]
  (.setValue slider num-value))

(defmethod set-control-value! TextField [^TextField text-field value]
  (.setText text-field (cond-> value (number? value) field-expression/format-number))
  (.fireEvent text-field (KeyEvent.
                           #_source text-field
                           #_target text-field
                           #_eventType KeyEvent/KEY_PRESSED
                           #_character ""
                           #_text "\r"
                           #_code KeyCode/ENTER
                           #_shiftDown false
                           #_controlDown false
                           #_altDown false
                           #_metaDown false)))

(defmethod set-control-value! ToggleButton [^ToggleButton toggle-button _num-value]
  (.fire toggle-button))

(defonce temp-directory-path
  (memoize
    (fn temp-directory-path []
      (let [temp-file (fs/create-temp-file!)
            parent-directory-path (.getCanonicalPath (.getParentFile temp-file))]
        (fs/delete! temp-file {:fail :silently})
        parent-directory-path))))

(defn make-directory-deleter
  "Returns an AutoCloseable that deletes the directory at the specified
  path when closed. Suitable for use with the (with-open) macro. The
  directory path must be a temp directory."
  ^AutoCloseable [directory-path]
  (let [directory (io/file directory-path)]
    (assert (string/starts-with? (.getCanonicalPath directory)
                                 (temp-directory-path))
            (str "directory-path `" (.getCanonicalPath directory) "` is not a temp directory"))
    (reify AutoCloseable
      (close [_]
        (fs/delete-directory! directory {:fail :silently})))))

(defn make-dir! ^File [^File dir]
  (fs/create-directory! dir))

(defn make-temp-dir! ^File []
  (fs/create-temp-directory! "foo"))

(defmacro with-temp-dir!
  "Creates a temporary directory and binds it to name as a File. Executes body
  in a try block and finally deletes the temporary directory."
  [name & body]
  `(let [~name (make-temp-dir!)]
     (try
       ~@body
       (finally
         (fs/delete! ~name {:fail :silently})))))

(defn- file-tree-helper [^File entry]
  (if (.isDirectory entry)
    {(.getName entry)
     (mapv file-tree-helper (sort-by #(.getName ^File %) (filter #(not (.isHidden ^File %)) (.listFiles entry))))}
    (.getName entry)))

(defn file-tree
  "Returns a vector of file tree entries below dir. A file entry is represented
  as a String file name. A directory entry is represented as a single-entry map
  where the key is the directory name and the value is a vector of tree entries."
  [^File dir]
  (second (first (file-tree-helper dir))))

(defn make-file-tree!
  "Takes a vector of file tree entries and creates a mock directory structure
  below the specified directory entry. A random UUID string will be written to
  each file."
  [^File entry file-tree]
  (doseq [decl file-tree]
    (if (map? decl)
      (let [[^String dir-name children] (first decl)
            dir-entry (make-dir! (io/file entry dir-name))]
        (make-file-tree! dir-entry children))
      (let [^String file-name decl]
        (spit (io/file entry file-name) (.toString (UUID/randomUUID)))))))

(defn make-build-stage-test-prefs []
  (prefs/global "test/resources/test.editor_settings"))

(defonce shared-test-prefs-file (fs/create-temp-file! "unit-test" "prefs.editor_settings"))
(defn make-test-prefs []
  (prefs/make :scopes {:global shared-test-prefs-file :project shared-test-prefs-file}
              :schemas [:default]))

(def localization
  (localization/make (make-test-prefs) ::test {"en.editor_localization" #(io/reader (io/resource "localization/en.editor_localization"))}))

(declare resolve-prop)

(defn code-editor-lines [script-id]
  (let [[node-id] (resolve-prop script-id :modified-lines)]
    (g/node-value node-id :lines)))

(defn code-editor-text
  ^String [script-id]
  (code.data/lines->string (code-editor-lines script-id)))

(defn set-code-editor-lines [script-id lines]
  (let [[node-id label] (resolve-prop script-id :modified-lines)]
    (g/set-property node-id label lines)))

(defn set-code-editor-lines! [script-id lines]
  (g/transact (set-code-editor-lines script-id lines)))

(defn update-code-editor-lines [script-id f & args]
  (let [old-lines (code-editor-lines script-id)
        new-lines (apply f old-lines args)]
    (set-code-editor-lines script-id new-lines)))

(defn update-code-editor-lines! [script-id f & args]
  (g/transact (apply update-code-editor-lines script-id f args)))

(defn set-code-editor-source [script-id source]
  (let [lines (cond
                (string? source) (code.data/string->lines source)
                (vector? source) source
                :else (throw (ex-info "source must be a string or a vector of lines."
                                      {:source source})))]
    (set-code-editor-lines script-id lines)))

(defn set-code-editor-source! [script-id source]
  (g/transact (set-code-editor-source script-id source)))

(defn lua-module-text
  ^String [lua-module]
  {:pre [(map? lua-module)]} ; Lua$LuaModule in map format.
  (let [^ByteString lua-source-byte-string (-> lua-module :source :script)]
    (.toStringUtf8 lua-source-byte-string)))

(defn lua-module-lines [lua-module]
  {:pre [(map? lua-module)]} ; Lua$LuaModule in map format.
  (-> lua-module
      lua-module-text
      code.data/string->lines))

(defn set-non-editable-directories! [project-path non-editable-directory-proj-paths]
  {:pre [(seqable? non-editable-directory-proj-paths)
         (every? string? non-editable-directory-proj-paths)]}
  (test-support/spit-until-new-mtime
    (shared-editor-settings/shared-editor-settings-file project-path)
    (shared-editor-settings/map->save-data-content
      (cond-> {}
              (seq non-editable-directory-proj-paths)
              (assoc :non-editable-directories (vec non-editable-directory-proj-paths))))))

(defn write-defunload-patterns!
  ^File [project-path patterns]
  (fs/create-file!
    (io/file project-path ".defunload")
    (string/join (System/getProperty "line.separator")
                 patterns)))

(defn- proj-path-resource-type [basis workspace proj-path]
  (let [editable-proj-path? (g/raw-property-value basis workspace :editable-proj-path?)
        editability (editable-proj-path? proj-path)
        type-ext (resource/filename->type-ext proj-path)]
    (workspace/get-resource-type workspace editability type-ext)))

(defn write-file-resource!
  ^File [workspace proj-path save-value]
  {:pre [(string/starts-with? proj-path "/")]}
  (let [basis (g/now)
        resource-type (proj-path-resource-type basis workspace proj-path)
        project-directory (workspace/project-directory basis workspace)
        file (io/file project-directory (subs proj-path 1))
        write-fn (:write-fn resource-type)
        contents (write-fn save-value)]
    (test-support/write-until-new-mtime
      file contents)
    file))

(defn setup-workspace!
  ([graph]
   (setup-workspace! graph project-path))
  ([graph project-path]
   (let [workspace-config (shared-editor-settings/load-project-workspace-config project-path localization)
         workspace (workspace/make-workspace graph
                                             project-path
                                             {}
                                             workspace-config
                                             localization)]
     (g/transact
       (concat
         (scene/register-view-types workspace)))
     (resource-types/register-resource-types! workspace)
     (workspace/resource-sync! workspace)
     workspace)))

(defn make-temp-project-copy! [project-path]
  (let [temp-project-path (-> (fs/create-temp-directory! "test")
                              (.getCanonicalPath))]
    (fs/copy-directory! (io/file project-path) (io/file temp-project-path))
    temp-project-path))

(defn setup-scratch-workspace!
  ([graph]
   (setup-scratch-workspace! graph project-path))
  ([graph project-path]
   (let [temp-project-path (make-temp-project-copy! project-path)]
     (setup-workspace! graph temp-project-path))))

(defn fetch-libraries! [workspace]
  (let [game-project-resource (workspace/find-resource workspace "/game.project")
        dependencies (project/read-dependencies game-project-resource)]
    (->> (workspace/fetch-and-validate-libraries workspace dependencies progress/null-render-progress!)
         (workspace/install-validated-libraries! workspace))
    (workspace/resource-sync! workspace [] progress/null-render-progress!)))

(defn set-libraries! [workspace library-uris]
  (let [library-uris
        (mapv (fn [value]
                (cond
                  (instance? URI value) value
                  (instance? URL value) (.toURI ^URL value)
                  (string? value) (.toURI (URL. ^String value))
                  :else (throw (ex-info "library-uris contain invalid values."
                                        {:library-uris library-uris}))))
              library-uris)]
    (->> (workspace/fetch-and-validate-libraries workspace library-uris progress/null-render-progress!)
         (workspace/install-validated-libraries! workspace))
    (workspace/resource-sync! workspace [] progress/null-render-progress!)))

(defn distinct-resource-types-by-editability
  ([workspace]
   (distinct-resource-types-by-editability workspace fn/constantly-true))
  ([workspace resource-type-predicate]
   (let [editable-protobuf-resource-types
         (into (sorted-map)
               (filter (fn [[_ext editable-resource-type]]
                         (resource-type-predicate editable-resource-type)))
               (workspace/get-resource-type-map workspace :editable))

         distinctly-non-editable-protobuf-resource-types
         (into (sorted-map)
               (filter (fn [[ext non-editable-resource-type]]
                         (and (resource-type-predicate non-editable-resource-type)
                              (not (identical? non-editable-resource-type
                                               (editable-protobuf-resource-types ext))))))
               (workspace/get-resource-type-map workspace :non-editable))]

     {:editable (mapv val editable-protobuf-resource-types)
      :non-editable (mapv val distinctly-non-editable-protobuf-resource-types)})))

(defn setup-project!
  ([workspace]
   (let [proj-graph (g/make-graph! :history true :volatility 1)
         extensions (extensions/make proj-graph)
         project (project/make-project proj-graph workspace extensions)
         project (project/load-project! project)]
     (g/reset-undo! proj-graph)
     project))
  ([workspace resources]
   (let [proj-graph (g/make-graph! :history true :volatility 1)
         extensions (extensions/make proj-graph)
         project (project/make-project proj-graph workspace extensions)
         project (project/load-project! project progress/null-render-progress! resources)]
     (g/reset-undo! proj-graph)
     project)))

(defn project-node-resources [project]
  (->> (g/node-value project :node-id+resources)
       (map second)
       (sort-by resource/proj-path)))

(defrecord FakeFileResource [workspace root ^File file children exists? source-type read-only? loaded? content]
  resource/Resource
  (children [this] children)
  (ext [this] (FilenameUtils/getExtension (.getPath file)))
  (resource-type [this] (resource/lookup-resource-type (g/now) workspace this))
  (source-type [this] source-type)
  (exists? [this] exists?)
  (read-only? [this] read-only?)
  (path [this] (if (= "" (.getName file)) "" (resource/relative-path (io/file ^String root) file)))
  (abs-path [this] (.getAbsolutePath  file))
  (proj-path [this] (if (= "" (.getName file)) "" (str "/" (resource/path this))))
  (resource-name [this] (.getName file))
  (workspace [this] workspace)
  (resource-hash [this] (hash (resource/proj-path this)))
  (openable? [this] (= :file source-type))
  (editable? [this] true)
  (loaded? [_this] loaded?)

  io/IOFactory
  (make-input-stream  [this opts] (io/make-input-stream content opts))
  (make-reader        [this opts] (io/make-reader (io/make-input-stream this opts) opts))
  (make-output-stream [this opts] (assert false "writing to not supported"))
  (make-writer        [this opts] (assert false "writing to not supported")))

(defn make-fake-file-resource
  ([workspace root file content]
   (make-fake-file-resource workspace root file content nil))
  ([workspace root file content {:keys [children exists? source-type read-only? loaded?]
                                 :or {children nil
                                      exists? true
                                      source-type :file
                                      read-only? false
                                      loaded? true}
                                 :as opts}]
   (FakeFileResource. workspace root file children exists? source-type read-only? loaded? content)))

(defn resource-node
  "Returns the node-id of the ResourceNode matching the specified Resource or
  proj-path in the project. Throws an exception if no matching ResourceNode can
  be found in the project."
  [project resource-or-proj-path]
  (or (project/get-resource-node project resource-or-proj-path)
      (throw (ex-info (str "Resource node not found: " resource-or-proj-path)
                      {:project project
                       :resource-or-proj-path resource-or-proj-path}))))

(defn empty-selection? [app-view]
  (let [sel (g/node-value app-view :selected-node-ids)]
    (empty? sel)))

(defn selected? [app-view tgt-node-id]
  (let [sel (g/node-value app-view :selected-node-ids)]
    (not (nil? (some #{tgt-node-id} sel)))))

(g/defnode MockView
  (inherits view/WorkbenchView))

(g/defnode MockAppView
  (inherits app-view/AppView)
  (property active-view g/NodeID)
  (output active-view g/NodeID (gu/passthrough active-view)))

(defn make-view-graph! []
  (g/make-graph! :history false :volatility 2))

(defn setup-app-view! [project]
  (let [view-graph (make-view-graph!)]
    (-> (g/make-nodes view-graph [app-view [MockAppView
                                            :active-tool :move
                                            :manip-space :world
                                            :scene (Scene. (VBox.))]]
          (g/connect project :_node-id app-view :project-id)
          (for [label [:selected-node-ids-by-resource-node :selected-node-properties-by-resource-node :sub-selections-by-resource-node]]
            (g/connect project label app-view label)))
      g/transact
      g/tx-nodes-added
      first)))

(defn- make-tab! [project app-view path make-view-fn!]
  (let [node-id (project/get-resource-node project path)
        views-by-node-id (let [views (g/node-value app-view :open-views)]
                           (zipmap (map :resource-node (vals views)) (keys views)))
        view (get views-by-node-id node-id)]
    (if view
      (do
        (g/set-property! app-view :active-view view)
        [node-id view])
      (let [view-graph (g/make-graph! :history false :volatility 2)
            view (make-view-fn! view-graph node-id)]
        (g/transact
          (concat
            (g/connect node-id :_node-id view :resource-node)
            (g/connect node-id :valid-node-id+type+resource view :node-id+type+resource)
            (g/connect view :view-data app-view :open-views)
            (g/set-property app-view :active-view view)))
        (app-view/select! app-view [node-id])
        [node-id view]))))

(defn open-tab! [project app-view path]
  (first
    (make-tab! project app-view path (fn [view-graph resource-node]
                                      (->> (g/make-node view-graph MockView)
                                        g/transact
                                        g/tx-nodes-added
                                        first)))))

(defn open-scene-view! [project app-view path width height]
  (make-tab! project app-view path (fn [view-graph resource-node]
                                     (scene/make-preview view-graph resource-node {:prefs (make-build-stage-test-prefs) :app-view app-view :project project :select-fn (partial app-view/select app-view)} width height))))

(defn close-tab! [project app-view path]
  (let [node-id (project/get-resource-node project path)
        view (some (fn [[view-id {:keys [resource-node]}]]
                     (when (= resource-node node-id) view-id)) (g/node-value app-view :open-views))]
    (when view
      (g/delete-graph! (g/node-id->graph-id view)))))

(defn setup!
  ([graph]
   (setup! graph project-path))
  ([graph project-path]
   (let [workspace (setup-workspace! graph project-path)
         project (setup-project! workspace)
         app-view (setup-app-view! project)]
     [workspace project app-view])))

(defn- load-system-and-project-raw [path]
  (test-support/with-clean-system {:cache-size system-cache-size
                                   :cache-retain? project/cache-retain?}
    (let [workspace (setup-workspace! world path)]
      (fetch-libraries! workspace)
      (let [project (setup-project! workspace)]
        [@g/*the-system* workspace project]))))

(def load-system-and-project (fn/memoize load-system-and-project-raw))

(defn clear-cached-libraries! []
  (fn/clear-memoized! (var-get #'editor.library/fetch-library!)))

(defn clear-cached-projects! []
  (fn/clear-memoized! load-system-and-project))

(defn evict-cached-project! [path]
  (fn/evict-memoized! load-system-and-project path))

(defn cached-endpoints
  ([] (cached-endpoints (g/cache)))
  ([cache]
   (into (sorted-set)
         (map key)
         cache)))

(defn cacheable-save-data-endpoints
  ([node-id]
   (cacheable-save-data-endpoints (g/now) node-id))
  ([basis node-id]
   (let [node-type (g/node-type* basis node-id)
         output-cached? (g/cached-outputs node-type)]
     (into (sorted-set)
           (comp (filter output-cached?)
                 (map #(g/endpoint node-id %)))
           [:save-data :save-value]))))

(defn cacheable-save-data-outputs
  ([node-id]
   (cacheable-save-data-outputs (g/now) node-id))
  ([basis node-id]
   (into (sorted-set)
         (map g/endpoint-label)
         (cacheable-save-data-endpoints basis node-id))))

(defn cached-save-data-outputs
  ([node-id]
   (cached-save-data-outputs (g/cache) node-id))
  ([cache node-id]
   (into (sorted-set)
         (filter (fn [output-label]
                   (contains? cache (g/endpoint node-id output-label))))
         [:save-data :save-value])))

(defn uncached-save-data-outputs
  ([node-id]
   (uncached-save-data-outputs (g/now) (g/cache) node-id))
  ([basis node-id]
   (uncached-save-data-outputs basis (g/cache) node-id))
  ([basis cache node-id]
   (into (sorted-set)
         (comp (remove #(contains? cache %))
               (map g/endpoint-label))
         (cacheable-save-data-endpoints basis node-id))))

(defn uncached-save-data-outputs-by-proj-path
  ([project]
   (uncached-save-data-outputs-by-proj-path (g/now) (g/cache) project))
  ([basis project]
   (uncached-save-data-outputs-by-proj-path basis (g/cache) project))
  ([basis cache project]
   (into (sorted-map)
         (keep (fn [[node-id]]
                 (when-not (g/defective? basis node-id)
                   (let [resource (resource-node/resource basis node-id)
                         proj-path (resource/proj-path resource)]
                     (when-some [uncached-save-data-outputs (not-empty (uncached-save-data-outputs basis cache node-id))]
                       (pair proj-path uncached-save-data-outputs))))))
         (g/sources-of basis project :save-data))))

(defn- split-keyword-options [forms]
  (let [keyword-options (into {}
                              (comp (take-while (comp keyword? first))
                                    (map vec)) ; Convert lazy sequences to vectors required by (into {}).
                              (partition 2 forms))
        forms (drop (* 2 (count keyword-options))
                    forms)]
    [keyword-options forms]))

(defmacro with-loaded-project
  [& forms]
  (let [first-form (first forms)
        custom-path? (or (string? first-form) (symbol? first-form))
        project-path (if custom-path? first-form project-path)
        forms (if custom-path? (next forms) forms)
        [options forms] (split-keyword-options forms)]
    `(let [options# ~options
           [system# ~'workspace ~'project] (if (:logging-suppressed options#)
                                             (log/without-logging
                                               (load-system-and-project ~project-path))
                                             (load-system-and-project ~project-path))
           system-clone# (is/clone-system system#)
           ~'cache (:cache system-clone#)
           ~'world (g/node-id->graph-id ~'workspace)]
       (binding [g/*the-system* (atom system-clone#)]
         (let [~'app-view (setup-app-view! ~'project)]
           ~@forms)))))

(defmacro with-scratch-project
  [project-path & forms]
  (let [[options forms] (split-keyword-options forms)]
    `(let [options# ~options]
       (test-support/with-clean-system {:cache-size ~system-cache-size
                                        :cache-retain? project/cache-retain?}
         (let [~'workspace (setup-scratch-workspace! ~'world ~project-path)]
           (fetch-libraries! ~'workspace)
           (let [~'project (if (:logging-suppressed options#)
                             (log/without-logging
                               (setup-project! ~'workspace))
                             (setup-project! ~'workspace))
                 ~'app-view (setup-app-view! ~'project)]
             ~@forms))))))

(defmacro with-temp-project-content
  [save-values-by-proj-path & body]
  `(let [save-values-by-proj-path# ~save-values-by-proj-path
         ~'project-path (make-temp-project-copy! "test/resources/empty_project")]
     (with-open [project-directory-deleter# (make-directory-deleter ~'project-path)]
       (test-support/with-clean-system {:cache-size ~system-cache-size
                                        :cache-retain? project/cache-retain?}
         (let [~'workspace (setup-workspace! ~'world ~'project-path)]
           (doseq [[proj-path# save-value#] save-values-by-proj-path#]
             (write-file-resource! ~'workspace proj-path# save-value#))
           (workspace/resource-sync! ~'workspace)
           (fetch-libraries! ~'workspace)
           (let [~'project (setup-project! ~'workspace)
                 ~'app-view (setup-app-view! ~'project)]
             ~@body))))))

(defmacro with-ui-run-later-rebound
  [& forms]
  `(let [laters# (atom [])]
     (with-redefs [ui/do-run-later (fn ~'fake-run-later [f#] (swap! laters# conj f#))]
       (let [result# (do ~@forms)]
         (doseq [f# @laters#] (f#))
         result#))))

(defn run-event-loop!
  "Starts a simulated event loop and enqueues the supplied function on it.
  The function is invoked with a single argument, which is a function that must
  be called to exit the event loop. Blocks until the event loop is terminated,
  or an exception is thrown from an enqueued action. While the event loop is
  running, ui/run-now and ui/run-later are rebound to enqueue actions on the
  simulated event loop, and ui/on-ui-thread? will return true only if called
  from inside the event loop."
  [f]
  (let [ui-thread (Thread/currentThread)
        action-queue (LinkedBlockingQueue.)
        enqueue-action! (fn [action!]
                          (.add action-queue action!)
                          nil)
        exit-event-loop! #(enqueue-action! ::exit-event-loop)]
    (with-redefs [ui/on-ui-thread? #(= ui-thread (Thread/currentThread))
                  ui/do-run-later enqueue-action!]
      (enqueue-action! (fn [] (f exit-event-loop!)))
      (loop []
        (let [action! (.take action-queue)]
          (when (not= ::exit-event-loop action!)
            (action!)
            (recur)))))))

(defn set-active-tool! [app-view tool]
  (g/transact (g/set-property app-view :active-tool tool)))

(defn- fake-input!
  ([view type x y]
   (fake-input! view type x y []))
  ([view type x y modifiers]
   (fake-input! view type x y modifiers 0))
  ([view type x y modifiers click-count]
   (fake-input! view type x y modifiers click-count :primary))
  ([view type x y modifiers click-count button]
   (let [pos [x y 0.0]]
     (g/transact (g/set-property view :tool-picking-rect (scene-selection/calc-picking-rect pos pos))))
   (let [handlers (g/sources-of view :input-handlers)
         user-data (g/node-value view :selected-tool-renderables)
         action (reduce #(assoc %1 %2 true)
                        {:type type :x x :y y :click-count click-count :button button}
                        modifiers)
         action (scene/augment-action view action)]
     (scene/dispatch-input handlers action user-data))))

(defn mouse-press!
  ([view x y]
   (fake-input! view :mouse-pressed x y))
  ([view x y modifiers]
   (fake-input! view :mouse-pressed x y modifiers 0))
  ([view x y modifiers click-count]
   (fake-input! view :mouse-pressed x y modifiers click-count)))

(defn mouse-move! [view x y]
  (fake-input! view :mouse-moved x y))

(defn mouse-release! [view x y]
  (fake-input! view :mouse-released x y))

(defn mouse-click!
  ([view x y]
   (mouse-click! view x y []))
  ([view x y modifiers]
   (mouse-click! view x y modifiers 0))
  ([view x y modifiers click-count]
   (mouse-press! view x y modifiers (inc click-count))
   (mouse-release! view x y)))

(defn mouse-dbl-click!
  ([view x y]
   (mouse-dbl-click! view x y []))
  ([view x y modifiers]
   (mouse-click! view x y modifiers)
   (mouse-click! view x y modifiers 1)))

(defn mouse-drag! [view x0 y0 x1 y1]
  (mouse-press! view x0 y0)
  (mouse-move! view x1 y1)
  (mouse-release! view x1 y1))

(defn manip-move! [scene-node-id offset-xyz]
  {:pre [(vector? offset-xyz)]}
  (g/transact
    (g/with-auto-evaluation-context evaluation-context
      (scene-tools/manip-move evaluation-context scene-node-id (doto (Vector3d.) (math/clj->vecmath offset-xyz))))))

(defn manip-rotate! [scene-node-id euler-xyz]
  {:pre [(vector? euler-xyz)]}
  (g/transact
    (g/with-auto-evaluation-context evaluation-context
      (scene-tools/manip-rotate evaluation-context scene-node-id (math/euler->quat euler-xyz)))))

(defn manip-scale! [scene-node-id scale-xyz]
  {:pre [(vector? scale-xyz)]}
  (g/transact
    (g/with-auto-evaluation-context evaluation-context
      (scene-tools/manip-scale evaluation-context scene-node-id (doto (Vector3d.) (math/clj->vecmath scale-xyz))))))

(defn dump-frame! [view path]
  (let [^BufferedImage image (g/node-value view :frame)]
    (let [file (io/file path)]
      (ImageIO/write image "png" file))))

(defn outline [root path]
  (get-in (g/node-value root :node-outline) (interleave (repeat :children) path)))

(defrecord OutlineItemIterator [root path]
  outline/ItemIterator
  (value [_this]
    (outline root path))
  (parent [_this]
    (when (not (empty? path))
      (->OutlineItemIterator root (butlast path)))))

(defn- make-outline-item-iterator [root-node-id outline-path]
  {:pre [(g/node-id? root-node-id)
         (vector? outline-path)
         (every? nat-int? outline-path)]}
  (->OutlineItemIterator root-node-id outline-path))

(defn outline-node-info
  "Given a node-id, evaluate its :node-outline output and locate the child
  element addressed by the supplied path of outline-label strings. Return the
  :node-outline info at the resulting path. Throws an exception if the path does
  not lead up to a valid node."
  [node-id & outline-labels]
  {:pre [(every? (some-fn string? localization/message-pattern?) outline-labels)]}
  (reduce (fn [node-outline outline-label]
            (or (some (fn [child-outline]
                        (when (= outline-label (:label child-outline))
                          child-outline))
                      (:children node-outline))
                (let [candidates (into (sorted-set)
                                       (map (comp localization :label))
                                       (:children node-outline))]
                  (throw (ex-info (format "node-outline for %s '%s' has no child-outline '%s'. Candidates: %s"
                                          (symbol (g/node-type-kw node-id))
                                          (:label node-outline)
                                          outline-label
                                          (string/join ", " (map #(str \' % \') candidates)))
                                  {:start-node-type-kw (g/node-type-kw node-id)
                                   :outline-labels (vec outline-labels)
                                   :failed-outline-label outline-label
                                   :failed-outline-label-candidates candidates
                                   :failed-node-outline node-outline})))))
          (g/valid-node-value node-id :node-outline)
          outline-labels))

(defn outline-node-id
  "Given a node-id, evaluate its :node-outline output and locate the child
  element addressed by the supplied path of outline-label strings. Return its
  node-id. Throws an exception if the path does not lead up to a valid node."
  [node-id & outline-labels]
  (:node-id (apply outline-node-info node-id outline-labels)))

(defn resource-outline-node-info
  "Looks up a resource node in the project, evaluates its :node-outline output,
  and locates the child element addressed by the supplied path of outline-label
  strings. Returns the :node-outline info at the resulting path. Throws an
  exception if the resource does not exist, or the outline-label path does not
  lead up to a valid node."
  [project resource-or-proj-path & outline-labels]
  (let [resource-node-id (resource-node project resource-or-proj-path)]
    (apply outline-node-info resource-node-id outline-labels)))

(defn resource-outline-node-id
  "Looks up a resource node in the project, evaluates its :node-outline output,
  and locates the child element addressed by the supplied path of outline-label
  strings. Returns its node-id. Throws an exception if the resource does not
  exist, or the outline-label path does not lead up to a valid node."
  [project resource-or-proj-path & outline-labels]
  (:node-id (apply resource-outline-node-info project resource-or-proj-path outline-labels)))

(defn outline-copy
  "Return a serialized a data representation of the specified parts of the
  edited scene. The returned value could be placed on the clipboard and later
  pasted into an edited scene."
  [project root-node-id & outline-paths]
  (let [item-iterators (mapv #(make-outline-item-iterator root-node-id %) outline-paths)]
    (outline/copy project item-iterators)))

(defn outline-cut!
  "Cut the specified parts of the edited scene and return a data representation
  of the cut nodes that can be placed on the clipboard."
  [project root-node-id outline-path & mode-outline-paths]
  (let [item-iterators (into [(make-outline-item-iterator root-node-id outline-path)]
                             (map #(make-outline-item-iterator root-node-id %))
                             mode-outline-paths)]
    (outline/cut! project item-iterators)))

(defn outline-paste!
  "Paste the copied-data into the edited scene, similar to how the user would
  perform this operation in the editor."
  ([project root-node-id outline-path copied-data]
   (outline-paste! project root-node-id outline-path copied-data nil))
  ([project root-node-id outline-path copied-data select-fn]
   (let [item-iterator (make-outline-item-iterator root-node-id outline-path)]
     (assert (outline/paste? project item-iterator copied-data))
     (outline/paste! project item-iterator copied-data select-fn))))

(defn outline-duplicate!
  "Simulate a copy-paste operation of the specified node in-place, typically how
  a user might duplicate an element in the edited scene."
  ([project root-node-id outline-path]
   (outline-duplicate! project root-node-id outline-path nil))
  ([project root-node-id outline-path select-fn]
   (let [copied-data (outline-copy project root-node-id outline-path)]
     (outline-paste! project root-node-id outline-path copied-data select-fn))))

(defn- outline->str
  ([outline]
   (outline->str outline "" true))
  ([outline prefix recurse?]
   (if outline
     (format "%s%s [%d] [%s]%s%s"
             (if recurse? (str prefix "* ") "")
             (:label outline "<no-label>")
             (:node-id outline -1)
             (some-> (g/node-type* (:node-id outline -1))
                     deref
                     :name)
             (if (:alt-outline outline) (format " (ALT: %s)" (outline->str (:alt-outline outline) prefix false)) "")
             (if recurse?
               (string/join (map #(str "\n" (outline->str % (str prefix "  ") true)) (:children outline)))
               ""))
     "")))

(defn dump-outline [root path]
  (-> (outline root path)
    outline->str
    println))

(defn resolve-prop [node-id label]
  (let [prop (get-in (g/node-value node-id :_properties) [:properties label])
        resolved-node-id (:node-id prop)
        resolved-label (get prop :prop-kw label)]
    [resolved-node-id resolved-label]))

(defn prop [node-id label]
  (get-in (g/node-value node-id :_properties) [:properties label :value]))

(defn prop-error [node-id label]
  (get-in (g/node-value node-id :_properties) [:properties label :error]))

(defn prop! [node-id label val]
  (let [prop (get-in (g/node-value node-id :_properties) [:properties label])]
    (if-let [set-fn (-> prop :edit-type :set-fn)]
      (g/transact
        (g/with-auto-evaluation-context evaluation-context
          (set-fn evaluation-context (:node-id prop) (:value prop) val)))
      (let [[node-id label] (resolve-prop node-id label)]
        (g/set-property! node-id label val)))))

(defn prop-clear! [node-id label]
  (if-let [clear-fn (get-in (g/node-value node-id :_properties) [:properties label :edit-type :clear-fn])]
    (g/transact (clear-fn node-id label))
    (let [[node-id label] (resolve-prop node-id label)]
      (g/clear-property! node-id label))))

(defn prop-read-only? [node-id label]
  (get-in (g/node-value node-id :_properties) [:properties label :read-only?]))

(defn prop-overridden? [node-id label]
  (contains? (get-in (g/node-value node-id :_properties) [:properties label]) :original-value))

(defn resource [workspace path]
  (workspace/file-resource workspace path))

(defn file
  ^File [workspace ^String path]
  (File. (workspace/project-directory workspace) path))

(defn selection [app-view]
  (-> app-view
    app-view/->selection-provider
    handler/selection))

;; Extension library server

(def lib-server-handler
  (http-server/router-handler
    {"/lib/{*lib}"
     {"GET" (fn [request]
              (let [lib (-> request :path-params :lib)
                    path-offset (count (format "test/resources/%s/" lib))
                    ignored #{".internal" "build"}
                    file-filter (reify FilenameFilter
                                  (accept [this file name] (not (contains? ignored name))))
                    files (->> (tree-seq (fn [^File f] (.isDirectory f))
                                         (fn [^File f] (.listFiles f ^FilenameFilter file-filter))
                                         (io/file (format "test/resources/%s" lib)))
                               (filter (fn [^File f] (not (.isDirectory f)))))
                    baos (ByteArrayOutputStream.)]
                (with-open [out (ZipOutputStream. baos)]
                  (doseq [^File f files]
                    (with-open [in (FileInputStream. f)]
                      (let [entry (doto (ZipEntry. (subs (.getPath f) path-offset))
                                    (.setSize (.length f)))]
                        (.putNextEntry out entry)
                        (IOUtils/copy in out)
                        (.closeEntry out)))))
                (http-server/response 200 {"etag" "tag"} (.toByteArray baos))))}}))

(defn lib-server-uri [server lib]
  (format "%s/lib/%s" (http-server/local-url server) lib))

(defn handler-run [command command-contexts user-data]
  (let [command-contexts (handler/eval-contexts command-contexts true)]
    (-> (handler/active command command-contexts user-data)
      handler/run)))

(defn handler-options [command command-contexts user-data]
  (let [command-contexts (handler/eval-contexts command-contexts true)]
    (-> (handler/active command command-contexts user-data)
      handler/options)))

(defn handler-state [command command-contexts user-data]
  (let [command-contexts (handler/eval-contexts command-contexts true)]
    (-> (handler/active command command-contexts user-data)
      handler/state)))

(defmacro with-prop [binding & forms]
  (let [[node-id# property# value#] binding]
    `(let [old-value# (prop ~node-id# ~property#)]
       (prop! ~node-id# ~property# ~value#)
       (try
         ~@forms
         (finally
           (prop! ~node-id# ~property# old-value#))))))

(defn make-graph-reverter
  "Returns an AutoCloseable that reverts the specified graph to the state it was
  at construction time when its close method is invoked. Suitable for use with
  the (with-open) macro."
  ^AutoCloseable [graph-id]
  (let [initial-undo-stack-count (g/undo-stack-count graph-id)]
    (reify AutoCloseable
      (close [_]
        (loop [undo-stack-count (g/undo-stack-count graph-id)]
          (when (< initial-undo-stack-count undo-stack-count)
            (g/undo! graph-id)
            (recur (g/undo-stack-count graph-id))))))))

(defn make-project-graph-reverter
  "Returns an AutoCloseable that reverts the project graph to the state it was
  at construction time when its close method is invoked. Suitable for use with
  the (with-open) macro."
  ^AutoCloseable [project]
  {:pre [(g/node-instance? project/Project project)]}
  (let [project-graph-id (g/node-id->graph-id project)]
    (make-graph-reverter project-graph-id)))

(defn- throw-invalid-component-resource-node-id-exception [basis node-id]
  (throw (ex-info "The specified node cannot be resolved to a component ResourceNode."
                  {:node-id node-id
                   :node-type (g/node-type* basis node-id)})))

(defmacro with-changes-reverted
  "Evaluates the body expressions in a try expression, and reverts any changes
  to the project graph in the finally clause. Returns the result of the last
  body expression."
  [project & body]
  `(with-open [project-graph-reverter# (make-project-graph-reverter ~project)]
     ~@body))

(defn- validate-component-resource-node-id
  ([node-id]
   (validate-component-resource-node-id (g/now) node-id))
  ([basis node-id]
   (if (and (g/node-instance? basis resource-node/ResourceNode node-id)
            (let [resource (resource-node/resource basis node-id)
                  resource-type (resource/resource-type resource)]
              (contains? (:tags resource-type) :component)))
     node-id
     (throw-invalid-component-resource-node-id-exception basis node-id))))

(defn to-component-resource-node-id
  ([node-id]
   (to-component-resource-node-id (g/now) node-id))
  ([basis node-id]
   (condp = (g/node-instance-match basis node-id [resource-node/ResourceNode
                                                  game-object/EmbeddedComponent
                                                  game-object/ReferencedComponent])
     resource-node/ResourceNode
     (validate-component-resource-node-id basis node-id)

     game-object/EmbeddedComponent
     (validate-component-resource-node-id basis (g/node-feeding-into basis node-id :embedded-resource-id))

     game-object/ReferencedComponent
     (validate-component-resource-node-id basis (g/node-feeding-into basis node-id :source-resource))

     (throw-invalid-component-resource-node-id-exception basis node-id))))

(defn to-game-object-node-id
  ([node-id]
   (to-game-object-node-id (g/now) node-id))
  ([basis node-id]
   (condp = (g/node-instance-match basis node-id [game-object/GameObjectNode
                                                  collection/GameObjectInstanceNode])
     game-object/GameObjectNode
     node-id

     collection/GameObjectInstanceNode
     (g/node-feeding-into basis node-id :source-resource)

     (throw (ex-info "The specified node cannot be resolved to a GameObjectNode."
                     {:node-id node-id
                      :node-type (g/node-type* basis node-id)})))))

(defn to-collection-node-id
  ([node-id]
   (to-collection-node-id (g/now) node-id))
  ([basis node-id]
   (condp = (g/node-instance-match basis node-id [collection/CollectionNode
                                                  collection/CollectionInstanceNode])
     collection/CollectionNode
     node-id

     collection/CollectionInstanceNode
     (g/node-feeding-into basis node-id :source-resource)

     (throw (ex-info "The specified node cannot be resolved to a CollectionNode."
                     {:node-id node-id
                      :node-type (g/node-type* basis node-id)})))))

(defn- created-node [select-fn-call-logger]
  (let [calls (fn/call-logger-calls select-fn-call-logger)
        args (last calls)
        selection (first args)
        node-id (first selection)]
    node-id))

(defn add-referenced-component!
  "Adds a new instance of a referenced component to the specified game object
  node. Returns the id of the added ReferencedComponent node."
  [game-object-or-instance-id component-resource]
  (let [game-object-id (to-game-object-node-id game-object-or-instance-id)
        select-fn (fn/make-call-logger)]
    (game-object/add-referenced-component! game-object-id component-resource select-fn)
    (created-node select-fn)))

(defn add-embedded-component!
  "Adds a new instance of an embedded component to the specified game object
  node. Returns the id of the added EmbeddedComponent node."
  [game-object-or-instance-id resource-type]
  (let [game-object-id (to-game-object-node-id game-object-or-instance-id)
        select-fn (fn/make-call-logger)]
    (game-object/add-embedded-component! game-object-id resource-type select-fn)
    (created-node select-fn)))

(defn add-referenced-game-object!
  "Adds a new instance of a referenced game object to the specified collection
  node. Returns the id of the added ReferencedGOInstanceNode."
  ([collection-or-instance-id game-object-resource]
   (let [collection-id (to-collection-node-id collection-or-instance-id)]
     (add-referenced-game-object! collection-id collection-id game-object-resource)))
  ([collection-or-instance-id parent-id game-object-resource]
   (let [collection-id (to-collection-node-id collection-or-instance-id)
         select-fn (fn/make-call-logger)]
     (collection/add-referenced-game-object! collection-id parent-id game-object-resource select-fn)
     (created-node select-fn))))

(defn add-embedded-game-object!
  "Adds a new instance of an embedded game object to the specified collection
  node. Returns the id of the added EmbeddedGOInstanceNode."
  ([collection-or-instance-id]
   (let [collection-id (to-collection-node-id collection-or-instance-id)]
     (add-embedded-game-object! collection-id collection-id)))
  ([collection-or-instance-id parent-id]
   (let [collection-id (to-collection-node-id collection-or-instance-id)
         project (project/get-project collection-id)
         workspace (project/workspace project)
         select-fn (fn/make-call-logger)]
     (collection/add-embedded-game-object! workspace project collection-id parent-id select-fn)
     (created-node select-fn))))

(defn add-referenced-collection!
  "Adds a new instance of a referenced collection to the specified collection
  node. Returns the id of the added CollectionInstanceNode."
  [collection-or-instance-id collection-resource]
  (let [collection-id (to-collection-node-id collection-or-instance-id)
        id (resource/base-name collection-resource)
        transform-properties nil
        overrides nil
        select-fn (fn/make-call-logger)]
    (collection/add-referenced-collection! collection-id collection-resource id transform-properties overrides select-fn)
    (created-node select-fn)))

(defn- checked-source-node-ids
  ([node-id->target-node-id target-input expected-source-node-type unresolved-target-node-id]
   (checked-source-node-ids node-id->target-node-id target-input expected-source-node-type (g/now) unresolved-target-node-id))
  ([node-id->target-node-id target-input expected-source-node-type basis unresolved-target-node-id]
   (let [target-node-id (node-id->target-node-id basis unresolved-target-node-id)
         target-node-type (g/node-type* basis target-node-id)]
     (when-not (g/has-input? target-node-type target-input)
       (throw (ex-info "Target node does not have the required input."
                       {:target-node-id target-node-id
                        :target-node-type target-node-type
                        :required-target-input target-input})))
     (mapv (fn [[source-node-id _source-label]]
             (if (g/node-instance? basis expected-source-node-type source-node-id)
               source-node-id
               (throw (ex-info "Source node does not match the expected source node type."
                               {:source-node-id source-node-id
                                :source-node-type (g/node-type* basis source-node-id)
                                :expected-source-node-type expected-source-node-type}))))
           (g/sources-of basis target-node-id target-input)))))

(def single util/only-or-throw)

(def embedded-components (partial checked-source-node-ids to-game-object-node-id :embed-ddf game-object/EmbeddedComponent))

(def embedded-component (comp single embedded-components))

(def referenced-components (partial checked-source-node-ids to-game-object-node-id :ref-ddf game-object/ReferencedComponent))

(def referenced-component (comp single referenced-components))

(def embedded-game-objects (partial checked-source-node-ids to-collection-node-id :embed-inst-ddf collection/EmbeddedGOInstanceNode))

(def embedded-game-object (comp single embedded-game-objects))

(def referenced-game-objects (partial checked-source-node-ids to-collection-node-id :ref-inst-ddf collection/ReferencedGOInstanceNode))

(def referenced-game-object (comp single referenced-game-objects))

(def referenced-collections (partial checked-source-node-ids to-collection-node-id :ref-coll-ddf collection/CollectionInstanceNode))

(def referenced-collection (comp single referenced-collections))

(defonce ^:private png-image-bytes
  ;; Bytes representing a single-color 256 by 256 pixel PNG file.
  ;; Source: https://www.mjt.me.uk/posts/smallest-png/
  (byte-array [0x89 0x50 0x4E 0x47  0x0D 0x0A 0x1A 0x0A  0x00 0x00 0x00 0x0D  0x49 0x48 0x44 0x52
               0x00 0x00 0x01 0x00  0x00 0x00 0x01 0x00  0x01 0x03 0x00 0x00  0x00 0x66 0xBC 0x3A
               0x25 0x00 0x00 0x00  0x03 0x50 0x4C 0x54  0x45 0xB5 0xD0 0xD0  0x63 0x04 0x16 0xEA
               0x00 0x00 0x00 0x1F  0x49 0x44 0x41 0x54  0x68 0x81 0xED 0xC1  0x01 0x0D 0x00 0x00
               0x00 0xC2 0xA0 0xF7  0x4F 0x6D 0x0E 0x37  0xA0 0x00 0x00 0x00  0x00 0x00 0x00 0x00
               0x00 0xBE 0x0D 0x21  0x00 0x00 0x01 0x9A  0x60 0xE1 0xD5 0x00  0x00 0x00 0x00 0x49
               0x45 0x4E 0x44 0xAE  0x42 0x60 0x82]))

(defn make-png-resource!
  "Adds a PNG image file to the workspace. Returns the created FileResource."
  [workspace proj-path]
  (assert (integer? workspace))
  (assert (string/starts-with? proj-path "/"))
  (let [resource (resource workspace proj-path)]
    (fs/create-parent-directories! (io/as-file resource))
    (with-open [out (io/output-stream resource)]
      (.write out ^bytes png-image-bytes))
    resource))

(defn make-resource!
  "Adds a new file to the workspace. Returns the created FileResource."
  ([workspace proj-path]
   (assert (integer? workspace))
   (assert (string/starts-with? proj-path "/"))
   (let [resource (resource workspace proj-path)
         resource-type (resource/resource-type resource)
         template (workspace/template workspace resource-type)]
     (fs/create-parent-directories! (io/as-file resource))
     (spit resource template)
     resource))
  ([workspace proj-path pb-map]
   (assert (integer? workspace))
   (assert (string/starts-with? proj-path "/"))
   (assert (map? pb-map))
   (let [resource (resource workspace proj-path)
         resource-type (resource/resource-type resource)
         write-fn (:write-fn resource-type)
         content (write-fn pb-map)]
     (fs/create-parent-directories! (io/as-file resource))
     (spit resource content)
     resource)))

(defn make-resource-node!
  "Adds a new file to the project. Returns the node-id of the created resource."
  [project proj-path]
  (assert (integer? project))
  (let [workspace (project/workspace project)
        resource (make-resource! workspace proj-path)]
    (workspace/resource-sync! workspace)
    (project/get-resource-node project resource)))

(defn make-atlas-resource-node! [project proj-path]
  {:pre [(string/ends-with? proj-path ".atlas")]}
  (let [workspace (project/workspace project)
        image-resource (make-png-resource! workspace (string/replace proj-path #".atlas$" ".png"))
        atlas (make-resource-node! project proj-path)]
    (g/transact
      (atlas/add-images atlas [image-resource]))
    atlas))

(defn make-code-resource-node! [project proj-path source]
  (doto (make-resource-node! project proj-path)
    (set-code-editor-source! source)))

(defn move-file!
  "Moves or renames a file in the workspace, then performs a resource sync."
  [workspace ^String from-proj-path ^String to-proj-path]
  (let [from-file (file workspace from-proj-path)
        to-file (file workspace to-proj-path)]
    (fs/move-file! from-file to-file)
    (workspace/resource-sync! workspace [[from-file to-file]])))

(defn block-until
  "Blocks the calling thread until the supplied predicate is satisfied for the
  return value of the specified polling function or the timeout expires. Returns
  nil if the timeout expires, or the last returned value of poll-fn otherwise."
  [done? timeout-ms poll-fn! & args]
  (let [deadline (+ (System/nanoTime) (long (* timeout-ms 1000000)))]
    (loop []
      (thread-util/throw-if-interrupted!)
      (let [result (apply poll-fn! args)]
        (if (done? result)
          result
          (if (< (System/nanoTime) deadline)
            (recur)))))))

(defn test-uses-assigned-material
  [workspace project node-id material-prop shader-path gpu-texture-path]
  (let [material-node (project/get-resource-node project "/materials/test_samplers.material")]
    (testing "uses shader and texture params from assigned material "
      (with-prop [node-id material-prop (workspace/resolve-workspace-resource workspace "/materials/test_samplers.material")]
        (let [scene-data (g/node-value node-id :scene)]
          ;; Additional uniforms might be introduced during rendering.
          (is (= (dissoc (get-in scene-data shader-path) :uniforms)
                 (dissoc (g/node-value material-node :shader) :uniforms)))
          (is (= (dissoc (get-in scene-data (conj gpu-texture-path :params)) :default-tex-params)
                 (dissoc (material/sampler->tex-params (first (g/node-value material-node :samplers))) :default-tex-params))))))))

(defn- build-node-result! [resource-node]
  (let [project (project/get-project resource-node)
        workspace (project/workspace project)
        old-artifact-map (workspace/artifact-map workspace)]
    (g/with-auto-evaluation-context evaluation-context
      (build/build-project! project resource-node old-artifact-map nil evaluation-context))))

(defn build-node! [resource-node]
  (let [build-result (build-node-result! resource-node)]
    (when-some [error (:error build-result)]
      (throw (ex-info "Build produced an ErrorValue."
                      {:resource resource
                       :node-type-kw (g/node-type-kw resource-node)
                       :error error})))
    build-result))

(defn build!
  ^AutoCloseable [resource-node]
  (let [resource (resource-node/resource resource-node)
        workspace (resource/workspace resource)
        build-directory (workspace/build-path workspace)]
    (build-node! resource-node)
    (make-directory-deleter build-directory)))

(defn build-error! [resource-node]
  (let [resource (resource-node/resource resource-node)
        workspace (resource/workspace resource)
        build-directory (workspace/build-path workspace)
        build-result (build-node-result! resource-node)]
    (fs/delete-directory! build-directory {:fail :silently})
    (:error build-result)))

(defn resolve-build-dependencies
  "Returns a flat list of dependent build targets"
  [node-id project]
  (let [ret (build/resolve-node-dependencies node-id project)]
    (when-not (is (not (g/error-value? ret)))
      (let [path (some-> (g/maybe-node-value node-id :resource) resource/proj-path)
            cause (->> ret
                       (tree-seq :causes :causes)
                       (some :message))]
        (throw (ex-info (str "Failed to build"
                             (when path (str " " path))
                             (when cause (str ": " cause)))
                        {:error ret}))))
    ret))

(defn node-build-resource [node-id]
  (:resource (first (g/node-value node-id :build-targets))))

(defn build-resource [project path]
  (when-some [resource-node (resource-node project path)]
    (node-build-resource resource-node)))

(defn nth-dep-build-resource [index project path]
  (when-some [resource-node (resource-node project path)]
    (:resource (nth (:deps (first (g/node-value resource-node :build-targets))) index))))

(def texture-build-resource (partial nth-dep-build-resource 0))
(def shader-program-build-resource (partial nth-dep-build-resource 0))

(defn build-output
  ^bytes [project path]
  (with-open [in (io/input-stream (build-resource project path))
              out (ByteArrayOutputStream.)]
    (io/copy in out)
    (.toByteArray out)))

(defn node-build-output
  ^bytes [node-id]
  (with-open [in (io/input-stream (node-build-resource node-id))
              out (ByteArrayOutputStream.)]
    (io/copy in out)
    (.toByteArray out)))

(defn node-built-build-resources
  "Returns the set of all BuildResources that will potentially be written to
  when building the specified node-id."
  [node-id]
  (into #{}
        (map :resource)
        (build/resolve-node-dependencies node-id (project/get-project node-id))))

(defn node-built-source-paths
  "Returns the set of all source resource proj-paths that will be built when
  building the specified node-id."
  [node-id]
  (into #{}
        (keep (comp resource/proj-path :resource :resource))
        (build/resolve-node-dependencies node-id (project/get-project node-id))))

(defmacro saved-pb [node-id pb-class]
  (with-meta `(protobuf/str->pb ~pb-class (resource-node/save-data-content (g/node-value ~node-id :save-data)))
             {:tag pb-class}))

(defmacro built-pb [node-id pb-class]
  (with-meta `(protobuf/bytes->pb ~pb-class (node-build-output ~node-id))
             {:tag pb-class}))

(defn- make-build-output-infos-by-path-impl [workspace resource-types-by-build-ext ^String build-output-path]
  (let [build-ext (resource/filename->type-ext build-output-path)
        resource-type (some (fn [[_ resource-type]]
                              (when (= build-ext (:build-ext resource-type))
                                resource-type))
                            (workspace/get-resource-type-map workspace))
        test-info (:test-info resource-type)
        pb-class (case (:type test-info)
                   (:code :ddf) (:built-pb-class test-info)
                   nil)
        built-file (workspace/build-path workspace build-output-path)
        built-bytes (Files/readAllBytes (.toPath built-file))
        build-output-info {:path build-output-path
                           :file built-file
                           :resource-type resource-type
                           :bytes built-bytes}]
    (assert (some? resource-type) (format "Unknown resource type for: '%s'" build-output-path))
    (if (nil? pb-class)
      (sorted-map build-output-path build-output-info)
      (let [dependencies-fn (resource-node/make-ddf-dependencies-fn pb-class)
            pb (protobuf/bytes->pb pb-class built-bytes)
            pb-map (protobuf/pb->map-without-defaults pb)
            dep-build-resource-paths (into (sorted-set)
                                           (dependencies-fn pb-map))]
        (into (sorted-map
                build-output-path
                (assoc build-output-info
                  :pb-class pb-class
                  :pb pb
                  :pb-map pb-map
                  :dep-paths dep-build-resource-paths))
              (mapcat #(make-build-output-infos-by-path-impl workspace resource-types-by-build-ext %))
              dep-build-resource-paths)))))

(defn make-build-output-infos-by-path
  "After a project has been built (i.e. the build directory has been populated),
  get info from the built binaries and the built binaries it depends on. Returns
  a map of build output proj-paths to a build-output-info map regarding that
  build output. The build-output-info map will always contain at least
  information about the output file, its resource-type, and its bytes on disk.
  For protobuf resources, the build-output-info map will also contain the
  decoded protobuf data, the build-output-paths it depends on, and the resulting
  map will include build-output-info entries for all the dependencies."
  [workspace ^String build-output-path]
  {:pre [(string? build-output-path)
         (string/starts-with? build-output-path "/")]}
  (let [resource-types-by-build-ext
        (into {}
              (map (fn [[_ {:keys [build-ext] :as resource-type}]]
                     (assert (string? build-ext))
                     (pair build-ext resource-type)))
              (workspace/get-resource-type-map workspace))]
    (make-build-output-infos-by-path-impl workspace resource-types-by-build-ext build-output-path)))

(defn unpack-property-declarations [property-declarations]
  {:pre [(or (nil? property-declarations) (map? property-declarations))]}
  (into {}
        (mapcat (fn [[entries-key values-key]]
                  (let [entries (get property-declarations entries-key)
                        values (get property-declarations values-key)]
                    (map (fn [{:keys [key index]}]
                           [key (values index)])
                         entries))))
        (vals properties/type->entry-keys)))

(defn first-subnode-of-type
  ([resource-node-id subnode-type]
   (g/with-auto-evaluation-context evaluation-context
     (first-subnode-of-type resource-node-id subnode-type evaluation-context)))
  ([resource-node-id subnode-type {:keys [basis] :as evaluation-context}]
   (or (some (fn [{:keys [node-id]}]
               (when (g/node-instance? basis subnode-type node-id)
                 node-id))
             (tree-seq :children :children
                       (g/valid-node-value resource-node-id :node-outline evaluation-context)))
       (throw (ex-info "No subnode matches the specified node type."
                       {:subnode-type (:k subnode-type)
                        :node-type (g/node-type-kw basis resource-node-id)
                        :proj-path (resource/resource->proj-path (resource-node/as-resource-original basis resource-node-id))})))))

(defn- get-setting-impl [form-data setting-path evaluation-context]
  (let [user-data (:user-data (:form-ops form-data))
        resource-setting-nodes (:resource-setting-nodes user-data)]
    (if-let [resource-setting-node-id (resource-setting-nodes setting-path)]
      (g/node-value resource-setting-node-id :value evaluation-context)
      (let [value (get (:values form-data) setting-path ::not-found)]
        (if (identical? ::not-found value)
          (settings-core/get-default-setting (:meta-settings form-data) setting-path)
          value)))))

(defn- with-setting
  [settings-resource-node-id setting-path evaluation-context form-data->result]
  (let [form-data (g/valid-node-value settings-resource-node-id :form-data evaluation-context)
        meta-settings (:meta-settings form-data)]
    (if (or (some #(= setting-path (:path %)) meta-settings))
      (form-data->result form-data)
      (throw (ex-info "Invalid setting path."
                      {:setting-path setting-path
                       :candidates (into (sorted-set)
                                         (map :path)
                                         meta-settings)})))))

(defn get-setting
  ([settings-resource-node-id setting-path]
   (g/with-auto-evaluation-context evaluation-context
     (get-setting settings-resource-node-id setting-path evaluation-context)))
  ([settings-resource-node-id setting-path evaluation-context]
   (with-setting
     settings-resource-node-id setting-path evaluation-context
     (fn get-setting-value [form-data]
       (get-setting-impl form-data setting-path evaluation-context)))))

(defn set-setting
  ([settings-resource-node-id setting-path value]
   (g/with-auto-evaluation-context evaluation-context
     (set-setting settings-resource-node-id setting-path value evaluation-context)))
  ([settings-resource-node-id setting-path value evaluation-context]
   (with-setting
     settings-resource-node-id setting-path evaluation-context
     (fn make-set-tx-data [form-data]
       (let [user-data (:user-data (:form-ops form-data))]
         (settings/set-tx-data user-data setting-path value))))))

(defn set-setting!
  ([settings-resource-node-id setting-path value]
   (g/with-auto-evaluation-context evaluation-context
     (set-setting! settings-resource-node-id setting-path value evaluation-context)))
  ([settings-resource-node-id setting-path value evaluation-context]
   (g/transact
     (set-setting settings-resource-node-id setting-path value evaluation-context))))

(defn update-setting [settings-resource-node-id setting-path transform-fn & args]
  (g/with-auto-evaluation-context evaluation-context
    (with-setting
      settings-resource-node-id setting-path evaluation-context
      (fn make-update-tx-data [form-data]
        (let [old-value (get-setting-impl form-data setting-path evaluation-context)
              new-value (apply transform-fn old-value args)
              user-data (:user-data (:form-ops form-data))]
          (settings/set-tx-data user-data setting-path new-value))))))

(defn clear-cached-save-data! [project]
  ;; Ensure any cache entries introduced by loading the project aren't covering
  ;; up an actual dirty-check issue.
  (project/clear-cached-save-data! project))

(defn save-project! [project]
  (let [workspace (project/workspace project)
        save-data (project/dirty-save-data project)
        post-save-actions (disk/write-save-data-to-disk! save-data nil nil)]
    (disk/process-post-save-actions! workspace post-save-actions)))

(defn dirty-proj-paths [project]
  (into (sorted-set)
        (map (comp resource/proj-path :resource))
        (project/dirty-save-data project)))

(defn dirty-proj-paths [project]
  (into (sorted-set)
        (map (comp resource/proj-path :resource))
        (project/dirty-save-data project)))

(defn type-preserving-add [a b]
  (condp instance? a
    Double (double (+ a b))
    Float (float (+ a b))
    Long (long (+ a b))
    Integer (int (+ a b))
    Short (short (+ a b))
    Byte (byte (+ a b))))

(defn- edit-multimethod-dispatch-fn [resource-node-id]
  (let [resource (resource-node/resource resource-node-id)
        resource-type (resource/resource-type resource)]
    (if (resource/placeholder-resource-type? resource-type)
      (if (text-util/binary? resource)
        :binary
        :code)
      (case (:type (:test-info resource-type))
        :code :code
        (let [ext (:ext resource-type)]
          (case ext
            ("tilegrid" "tilemap") "tilemap"
            ("tileset" "tilesource") "tilesource"
            ext))))))

(defmulti can-edit-resource-node? edit-multimethod-dispatch-fn)

(defmethod can-edit-resource-node? :default [_resource-node-id] true)

(defmethod can-edit-resource-node? "tpinfo" [_resource-node-id] false)

(defmulti edit-resource-node edit-multimethod-dispatch-fn)

(defmethod edit-resource-node :code [resource-node-id]
  (update-code-editor-lines resource-node-id conj ""))

(defmethod edit-resource-node "animationset" [resource-node-id]
  (g/update-property resource-node-id :animations pop))

(defmethod edit-resource-node "atlas" [resource-node-id]
  (g/update-property resource-node-id :margin type-preserving-add 1))

(defmethod edit-resource-node "camera" [resource-node-id]
  (g/update-property resource-node-id :fov type-preserving-add 1))

(defmethod edit-resource-node "collection" [resource-node-id]
  (let [instance-node-id (first-subnode-of-type resource-node-id collection/InstanceNode)]
    (g/update-property instance-node-id :id str \_)))

(defmethod edit-resource-node "collectionfactory" [resource-node-id]
  (g/set-property resource-node-id :prototype nil))

(defmethod edit-resource-node "collectionproxy" [resource-node-id]
  (g/set-property resource-node-id :collection nil))

(defmethod edit-resource-node "collisionobject" [resource-node-id]
  (g/update-property resource-node-id :linear-damping type-preserving-add 0.1))

(defmethod edit-resource-node "compute" [resource-node-id]
  (g/update-property resource-node-id :constants update-in [0 :name] str \_))

(defmethod edit-resource-node "convexshape" [resource-node-id]
  (g/update-property resource-node-id :pb update-in [:data 0] type-preserving-add 1))

(defmethod edit-resource-node "cubemap" [resource-node-id]
  (g/set-property resource-node-id :top nil))

(defmethod edit-resource-node "display_profiles" [resource-node-id]
  (let [profile-node-id (g/node-feeding-into resource-node-id :profile-msgs)]
    (g/update-property profile-node-id :name str \_)))

(defmethod edit-resource-node "factory" [resource-node-id]
  (g/set-property resource-node-id :prototype nil))

(defmethod edit-resource-node "font" [resource-node-id]
  (g/set-property resource-node-id :font nil))

(defmethod edit-resource-node "gamepads" [resource-node-id]
  (g/update-property resource-node-id :pb update-in [:driver 0 :dead-zone] type-preserving-add 0.1))

(defmethod edit-resource-node "go" [resource-node-id]
  (let [component-node-id (first-subnode-of-type resource-node-id game-object/ComponentNode)]
    (g/update-property component-node-id :id str \_)))

(defmethod edit-resource-node "gui" [resource-node-id]
  (g/update-property resource-node-id :max-nodes type-preserving-add 1))

(defmethod edit-resource-node "input_binding" [resource-node-id]
  (g/update-property resource-node-id :pb update-in [:mouse-trigger 0 :action] str \_))

(defmethod edit-resource-node "label" [resource-node-id]
  (g/update-property resource-node-id :tracking type-preserving-add 0.1))

(defmethod edit-resource-node "light" [resource-node-id]
  (g/update-property resource-node-id :pb update :range type-preserving-add 1))

(defmethod edit-resource-node "material" [resource-node-id]
  (g/update-property resource-node-id :tags conj "new_tag"))

(defmethod edit-resource-node "mesh" [resource-node-id]
  (g/update-property resource-node-id :position-stream str \_))

(defmethod edit-resource-node "model" [resource-node-id]
  (g/update-property resource-node-id :default-animation str \_))

(defmethod edit-resource-node "particlefx" [resource-node-id]
  (let [emitter-node-id (first-subnode-of-type resource-node-id particlefx/EmitterNode)]
    (g/update-property emitter-node-id :id str \_)))

(defmethod edit-resource-node "project" [resource-node-id]
  (update-setting resource-node-id ["project" "title"] str \_))

(defmethod edit-resource-node "render" [resource-node-id]
  (g/set-property resource-node-id :script nil))

(defmethod edit-resource-node "render_target" [resource-node-id]
  (g/update-property resource-node-id :color-attachments update-in [0 :width] type-preserving-add 1))

(defmethod edit-resource-node "rivemodel" [resource-node-id]
  (g/update-property resource-node-id :create-go-bones not))

(defmethod edit-resource-node "rivescene" [resource-node-id]
  (g/set-property resource-node-id :rive-file nil))

(defmethod edit-resource-node "simpledata" [resource-node-id]
  (g/update-property resource-node-id :i64 type-preserving-add 1))

(defmethod edit-resource-node "settings" [resource-node-id]
  (update-setting resource-node-id ["liveupdate" "zip-filepath"] str \_))

(defmethod edit-resource-node "shared_editor_settings" [resource-node-id]
  (update-setting resource-node-id ["performance" "cache_capacity"] type-preserving-add 1))

(defmethod edit-resource-node "sound" [resource-node-id]
  (g/update-property resource-node-id :speed type-preserving-add 0.1))

(defmethod edit-resource-node "spinemodel" [resource-node-id]
  (g/update-property resource-node-id :offset type-preserving-add 0.1))

(defmethod edit-resource-node "spinescene" [resource-node-id]
  (g/set-property resource-node-id :spine-json nil))

(defmethod edit-resource-node "sprite" [resource-node-id]
  (g/update-property resource-node-id :offset type-preserving-add 0.1))

(defmethod edit-resource-node "texture_profiles" [resource-node-id]
  (g/update-property resource-node-id :pb update-in [:profiles 0 :platforms 0 :max-texture-size] type-preserving-add 8))

(defmethod edit-resource-node "tilemap" [resource-node-id]
  (let [layer-node-id (first-subnode-of-type resource-node-id tile-map/LayerNode)]
    (g/update-property layer-node-id :z type-preserving-add 0.1)))

(defmethod edit-resource-node "tilesource" [resource-node-id]
  (g/update-property resource-node-id :tile-spacing type-preserving-add 1))

(defmethod edit-resource-node "tpatlas" [resource-node-id]
  (g/update-property resource-node-id :rename-patterns  str \_))

(defn edit-resource-node! [resource-node-id]
  (g/transact
    (edit-resource-node resource-node-id)))

(defn edit-proj-path! [project proj-path]
  (let [resource-node (resource-node project proj-path)]
    (edit-resource-node! resource-node)))

(defn protobuf-resource-exts-that-read-defaults [workspace]
  (into (sorted-set)
        (comp (mapcat #(vals (workspace/get-resource-type-map workspace %)))
              (filter (fn [{:keys [test-info]}]
                        (and (= :ddf (:type test-info))
                             (:read-defaults test-info))))
              (keep :ext))
        [:editable :non-editable]))

(defn- value-diff->string
  ^String [diff]
  (let [printer (deep-diff/printer {:print-color false
                                    :print-fallback :print})]
    (string/trim-newline
      (with-out-str
        (deep-diff/pretty-print diff printer)))))

(defn value-diff-message
  ^String [disk-value save-value]
  (str "Summary of discrepancies between disk value and save value:\n"
       (-> (deep-diff/diff disk-value save-value)
           (deep-diff/minimize)
           (value-diff->string))))

(defn text-diff-message
  ^String [disk-text save-text]
  (str "Summary of discrepancies between disk text and save text:\n"
       (or (some->> (diff/make-diff-output-lines disk-text save-text 3)
                    (string/join "\n"))
           "Contents are identical.")))

(defn check-value-equivalence! [expected-value actual-value message]
  (if (= expected-value actual-value)
    true
    (let [message-with-diff (str message \newline (value-diff-message expected-value actual-value))]
      (is (= expected-value actual-value) message-with-diff))))

(defn check-text-equivalence! [expected-text actual-text message]
  (if (= expected-text actual-text)
    true
    (let [message-with-diff (str message \newline (text-diff-message expected-text actual-text))]
      (is (= expected-text actual-text) message-with-diff))))

(defn save-data-diff-message
  ^String [save-data]
  (let [resource (:resource save-data)
        resource-type (resource/resource-type resource)
        read-fn (:read-fn resource-type)]
    (if read-fn
      ;; Compare data.
      (let [disk-value (resource-node/save-value->source-value (read-fn resource) resource-type)
            save-value (resource-node/save-value->source-value (:save-value save-data) resource-type)]
        (value-diff-message disk-value save-value))

      ;; Compare text.
      (let [disk-text (slurp resource)
            save-text (resource-node/save-data-content save-data)]
        (text-diff-message disk-text save-text)))))

(defn check-save-data-disk-equivalence! [save-data ^String project-path]
  (let [resource (:resource save-data)
        resource-type (resource/resource-type resource)
        read-fn (:read-fn resource-type)
        message (format "When checking `editor/%s%s`."
                        project-path
                        (resource/proj-path resource))

        are-values-equivalent
        (if-not read-fn
          false
          (let [disk-value (resource-node/save-value->source-value (read-fn resource) resource-type)
                save-value (resource-node/save-value->source-value (:save-value save-data) resource-type)]
            ;; We have a read-fn, compare data.
            (check-value-equivalence! disk-value save-value message)))]

    (when-not are-values-equivalent
      ;; We either don't have a read-fn, or the values differ.
      (let [disk-text (slurp resource)
            save-text (resource-node/save-data-content save-data)]
        ;; Compare text.
        (check-text-equivalence! disk-text save-text message)))))

(defmethod test/assert-expr 'thrown-with-data? [msg [_ expected-data-pred & body :as form]]
  `(try
     (do ~@body)
     (test/do-report {:type :fail :message ~msg :expected '~form :actual nil})
     (catch Throwable e#
       (let [actual-data# (ex-data e#)
             result# (if (~expected-data-pred actual-data#) :pass :fail)]
         (test/do-report {:type result# :message ~msg :expected '~form :actual e#})
         e#))))

(defmacro check-thrown-with-data! [expected-data-pred & body]
  `(is (~'thrown-with-data? ~expected-data-pred ~@body)))

(defmethod test/assert-expr 'thrown-with-root-cause-msg? [msg [_ re & body :as form]]
  `(try
     (do ~@body)
     (test/do-report {:type :fail :message ~msg :expected '~form :actual nil})
     (catch Throwable e#
       (let [^Throwable root# (loop [e# e#]
                                (if-let [cause# (ex-cause e#)]
                                  (recur cause#)
                                  e#))
             m# (.getMessage root#)]
         (if (re-find ~re m#)
           (test/do-report {:type :pass :message ~msg :expected '~form :actual e#})
           (test/do-report {:type :fail, :message ~msg, :expected '~form :actual e#})))
       e#)))

(defmacro check-thrown-with-root-cause-msg! [re & body]
  `(is (~'thrown-with-root-cause-msg? ~re ~@body)))
