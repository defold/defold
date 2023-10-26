;; Copyright 2020-2023 The Defold Foundation
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
            [clojure.test :refer [is testing]]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.atlas :as atlas]
            [editor.build :as build]
            [editor.code.data :as code.data]
            [editor.collection :as collection]
            [editor.defold-project :as project]
            [editor.disk :as disk]
            [editor.editor-extensions :as extensions]
            [editor.fs :as fs]
            [editor.game-object :as game-object]
            [editor.graph-util :as gu]
            [editor.handler :as handler]
            [editor.material :as material]
            [editor.pipeline :as pipeline]
            [editor.prefs :as prefs]
            [editor.progress :as progress]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.resource-types :as resource-types]
            [editor.scene :as scene]
            [editor.scene-selection :as scene-selection]
            [editor.settings :as settings]
            [editor.shared-editor-settings :as shared-editor-settings]
            [editor.ui :as ui]
            [editor.view :as view]
            [editor.workspace :as workspace]
            [internal.system :as is]
            [internal.util :as util]
            [service.log :as log]
            [support.test-support :as test-support]
            [util.fn :as fn]
            [util.http-server :as http-server]
            [util.thread-util :as thread-util])
  (:import [java.awt.image BufferedImage]
           [java.io ByteArrayOutputStream File FileInputStream FilenameFilter]
           [java.net URI URL]
           [java.util UUID]
           [java.util.concurrent LinkedBlockingQueue]
           [java.util.zip ZipEntry ZipOutputStream]
           [javafx.scene Scene]
           [javafx.scene.layout VBox]
           [javax.imageio ImageIO]
           [org.apache.commons.io FilenameUtils IOUtils]))

(set! *warn-on-reflection* true)

(def project-path "test/resources/test_project")

(def ^:private ^:const system-cache-size 1000)

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

(defn make-test-prefs []
  (prefs/load-prefs "test/resources/test_prefs.json"))

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

(defn setup-workspace!
  ([graph]
   (setup-workspace! graph project-path))
  ([graph project-path]
   (let [workspace-config (shared-editor-settings/load-project-workspace-config project-path)
         workspace (workspace/make-workspace graph
                                             project-path
                                             {}
                                             workspace-config)]
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

(defn setup-project!
  ([workspace]
   (let [proj-graph (g/make-graph! :history true :volatility 1)
         extensions (extensions/make proj-graph)
         project (project/make-project proj-graph workspace extensions)
         project (project/load-project project)]
     (g/reset-undo! proj-graph)
     project))
  ([workspace resources]
   (let [proj-graph (g/make-graph! :history true :volatility 1)
         extensions (extensions/make proj-graph)
         project (project/make-project proj-graph workspace extensions)
         project (project/load-project project resources)]
     (g/reset-undo! proj-graph)
     project)))

(defn project-node-resources [project]
  (g/with-auto-evaluation-context evaluation-context
    (sort-by resource/proj-path
             (map (comp #(g/node-value % :resource evaluation-context) first)
                  (g/sources-of project :node-id+resources)))))

(defrecord FakeFileResource [workspace root ^File file children exists? source-type read-only? content]
  resource/Resource
  (children [this] children)
  (ext [this] (FilenameUtils/getExtension (.getPath file)))
  (resource-type [this] (#'resource/get-resource-type workspace this))
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

  io/IOFactory
  (make-input-stream  [this opts] (io/make-input-stream content opts))
  (make-reader        [this opts] (io/make-reader (io/make-input-stream this opts) opts))
  (make-output-stream [this opts] (assert false "writing to not supported"))
  (make-writer        [this opts] (assert false "writing to not supported")))

(defn make-fake-file-resource
  ([workspace root file content]
   (make-fake-file-resource workspace root file content nil))
  ([workspace root file content {:keys [children exists? source-type read-only?]
                                 :or {children nil
                                      exists? true
                                      source-type :file
                                      read-only? false}
                                 :as opts}]
   (FakeFileResource. workspace root file children exists? source-type read-only? content)))

(defn resource-node [project path]
  (project/get-resource-node project path))

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
                                     (scene/make-preview view-graph resource-node {:prefs (make-test-prefs) :app-view app-view :project project :select-fn (partial app-view/select app-view)} width height))))

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

(defn evict-cached-system-and-project! [path]
  (fn/evict-memoized! load-system-and-project path))

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

(defmacro with-ui-run-later-rebound
  [& forms]
  `(let [laters# (atom [])]
     (with-redefs [ui/do-run-later (fn [f#] (swap! laters# conj f#))]
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
   (let [pos [x y 0.0]]
     (g/transact (g/set-property view :tool-picking-rect (scene-selection/calc-picking-rect pos pos))))
   (let [handlers (g/sources-of view :input-handlers)
         user-data (g/node-value view :selected-tool-renderables)
         action (reduce #(assoc %1 %2 true)
                        {:type type :x x :y y :click-count click-count}
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

(defn dump-frame! [view path]
  (let [^BufferedImage image (g/node-value view :frame)]
    (let [file (io/file path)]
      (ImageIO/write image "png" file))))

(defn outline [root path]
  (get-in (g/node-value root :node-outline) (interleave (repeat :children) path)))

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
  (let [[node-id label] (resolve-prop node-id label)]
    (g/set-property! node-id label val)))

(defn prop-clear! [node-id label]
  (let [[node-id label] (resolve-prop node-id label)]
    (g/clear-property! node-id label)))

(defn prop-read-only? [node-id label]
  (get-in (g/node-value node-id :_properties) [:properties label :read-only?]))

(defn prop-overridden? [node-id label]
  (contains? (get-in (g/node-value node-id :_properties) [:properties label]) :original-value))

(defn resource [workspace path]
  (workspace/file-resource workspace path))

(defn file ^File [workspace ^String path]
  (File. (workspace/project-path workspace) path))

(defn selection [app-view]
  (-> app-view
    app-view/->selection-provider
    handler/selection))

;; Extension library server

(defn ->lib-server []
  (doto (http-server/->server 0 {"/lib" (fn [request]
                                          (let [lib (subs (:url request) 5)
                                                path-offset (count (format "test/resources/%s/" lib))
                                                ignored #{".internal" "build"}
                                                file-filter (reify FilenameFilter
                                                              (accept [this file name] (not (contains? ignored name))))
                                                files (->> (tree-seq (fn [^File f] (.isDirectory f)) (fn [^File f] (.listFiles f file-filter)) (io/file (format "test/resources/%s" lib)))
                                                        (filter (fn [^File f] (not (.isDirectory f)))))]
                                            (with-open [byte-stream (ByteArrayOutputStream.)
                                                        out (ZipOutputStream. byte-stream)]
                                              (doseq [^File f files]
                                                (with-open [in (FileInputStream. f)]
                                                  (let [entry (doto (ZipEntry. (subs (.getPath f) path-offset))
                                                                (.setSize (.length f)))]
                                                    (.putNextEntry out entry)
                                                    (IOUtils/copy in out)
                                                    (.closeEntry out))))
                                              (.finish out)
                                              (let [bytes (.toByteArray byte-stream)]
                                                {:headers {"ETag" "tag"}
                                                 :body bytes}))))})
    (http-server/start!)))

(defn kill-lib-server [server]
  (http-server/stop! server))

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
    `(let [old-value# (g/node-value ~node-id# ~property#)]
       (g/set-property! ~node-id# ~property# ~value#)
       ~@forms
       (g/set-property! ~node-id# ~property# old-value#))))

(defn make-call-logger
  "Returns a function that keeps track of its invocations. Every
  time it is called, the call and its arguments are stored in the
  metadata associated with the returned function. If fn f is
  supplied, it will be invoked after the call is logged."
  ([]
   (make-call-logger (constantly nil)))
  ([f]
   (let [calls (atom [])]
     (with-meta (fn [& args]
                  (swap! calls conj args)
                  (apply f args))
                {::calls calls}))))

(defn call-logger-calls
  "Given a function obtained from make-call-logger, returns a
  vector of sequences containing the arguments for every time it
  was called."
  [call-logger]
  (-> call-logger meta ::calls deref))

(defmacro with-logged-calls
  "Temporarily redefines the specified functions into call-loggers
  while executing the body. Returns a map of functions to the
  result of (call-logger-calls fn). Non-invoked functions will not
  be included in the returned map.

  Example:
  (with-logged-calls [print println]
    (println :a)
    (println :a :b))
  => {#object[clojure.core$println] [(:a)
                                     (:a :b)]}"
  [var-symbols & body]
  `(let [binding-map# ~(into {}
                             (map (fn [var-symbol]
                                    `[(var ~var-symbol) (make-call-logger)]))
                             var-symbols)]
     (with-redefs-fn binding-map# (fn [] ~@body))
     (into {}
           (keep (fn [[var# call-logger#]]
                   (let [calls# (call-logger-calls call-logger#)]
                     (when (seq calls#)
                       [(deref var#) calls#]))))
           binding-map#)))

(def temp-directory-path
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
  ^java.lang.AutoCloseable [directory-path]
  (let [directory (io/file directory-path)]
    (assert (string/starts-with? (.getCanonicalPath directory)
                                 (temp-directory-path))
            (str "directory-path `" (.getCanonicalPath directory) "` is not a temp directory"))
    (reify java.lang.AutoCloseable
      (close [_]
        (fs/delete-directory! directory {:fail :silently})))))

(defn make-graph-reverter
  "Returns an AutoCloseable that reverts the specified graph to
  the state it was at construction time when its close method
  is invoked. Suitable for use with the (with-open) macro."
  ^java.lang.AutoCloseable [graph-id]
  (let [initial-undo-stack-count (g/undo-stack-count graph-id)]
    (reify java.lang.AutoCloseable
      (close [_]
        (loop [undo-stack-count (g/undo-stack-count graph-id)]
          (when (< initial-undo-stack-count undo-stack-count)
            (g/undo! graph-id)
            (recur (g/undo-stack-count graph-id))))))))

(defn- throw-invalid-component-resource-node-id-exception [basis node-id]
  (throw (ex-info "The specified node cannot be resolved to a component ResourceNode."
                  {:node-id node-id
                   :node-type (g/node-type* basis node-id)})))

(defn- validate-component-resource-node-id
  ([node-id]
   (validate-component-resource-node-id (g/now) node-id))
  ([basis node-id]
   (if (and (g/node-instance? basis resource-node/ResourceNode node-id)
            (when-some [resource-type (resource/resource-type (resource-node/resource basis node-id))]
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

     :else
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

     :else
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

     :else
     (throw (ex-info "The specified node cannot be resolved to a CollectionNode."
                     {:node-id node-id
                      :node-type (g/node-type* basis node-id)})))))

(defn- created-node [select-fn-call-logger]
  (let [calls (call-logger-calls select-fn-call-logger)
        args (last calls)
        selection (first args)
        node-id (first selection)]
    node-id))

(defn add-referenced-component!
  "Adds a new instance of a referenced component to the specified game object
  node. Returns the id of the added ReferencedComponent node."
  [game-object-or-instance-id component-resource]
  (let [game-object-id (to-game-object-node-id game-object-or-instance-id)
        select-fn (make-call-logger)]
    (game-object/add-referenced-component! game-object-id component-resource select-fn)
    (created-node select-fn)))

(defn add-embedded-component!
  "Adds a new instance of an embedded component to the specified game object
  node. Returns the id of the added EmbeddedComponent node."
  [game-object-or-instance-id resource-type]
  (let [game-object-id (to-game-object-node-id game-object-or-instance-id)
        select-fn (make-call-logger)]
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
         select-fn (make-call-logger)]
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
         select-fn (make-call-logger)]
     (collection/add-embedded-game-object! workspace project collection-id parent-id select-fn)
     (created-node select-fn))))

(defn add-referenced-collection!
  "Adds a new instance of a referenced collection to the specified collection
  node. Returns the id of the added CollectionInstanceNode."
  [collection-or-instance-id collection-resource]
  (let [collection-id (to-collection-node-id collection-or-instance-id)
        id (resource/base-name collection-resource)
        position [0.0 0.0 0.0]
        rotation [0.0 0.0 0.0 1.0]
        scale [1.0 1.0 1.0]
        overrides []
        select-fn (make-call-logger)]
    (collection/add-referenced-collection! collection-id collection-resource id position rotation scale overrides select-fn)
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

(defn- build-node! [resource-node]
  (let [project (project/get-project resource-node)
        workspace (project/workspace project)
        old-artifact-map (workspace/artifact-map workspace)
        build-path (workspace/build-path workspace)
        build-result (g/with-auto-evaluation-context evaluation-context
                       (build/build-project! project resource-node evaluation-context nil old-artifact-map progress/null-render-progress!))]
    [build-path build-result]))

(defn build!
  ^java.lang.AutoCloseable [resource-node]
  (let [[build-path] (build-node! resource-node)]
    (make-directory-deleter build-path)))

(defn build-error! [resource-node]
  (let [[build-path build-result] (build-node! resource-node)
        error (:error build-result)]
    (fs/delete-directory! (io/file build-path) {:fail :silently})
    error))

(defn node-build-resource [node-id]
  (:resource (first (g/node-value node-id :build-targets))))

(defn build-resource [project path]
  (when-some [resource-node (resource-node project path)]
    (node-build-resource resource-node)))

(defn nth-dep-build-resource [index project path]
  (when-some [resource-node (resource-node project path)]
    (:resource (nth (:deps (first (g/node-value resource-node :build-targets))) index))))

(def texture-build-resource (partial nth-dep-build-resource 0))
(def vertex-shader-build-resource (partial nth-dep-build-resource 0))
(def fragment-shader-build-resource (partial nth-dep-build-resource 1))

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
        (pipeline/flatten-build-targets
          (g/node-value node-id :build-targets))))

(defn node-built-source-paths
  "Returns the set of all source resource proj-paths that will be built when
  building the specified node-id."
  [node-id]
  (into #{}
        (keep (comp resource/proj-path :resource :resource))
        (pipeline/flatten-build-targets
          (g/node-value node-id :build-targets))))

(defmacro saved-pb [node-id pb-class]
  (with-meta `(protobuf/str->pb ~pb-class (:content (g/node-value ~node-id :undecorated-save-data)))
             {:tag pb-class}))

(defmacro built-pb [node-id pb-class]
  (with-meta `(protobuf/bytes->pb ~pb-class (node-build-output ~node-id))
             {:tag pb-class}))

(defn unpack-property-declarations [property-declarations]
  {:pre [(map? property-declarations)]}
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
                       (test-support/valid-node-value resource-node-id :node-outline evaluation-context)))
       (throw (ex-info "No subnode matches the specified node type."
                       {:subnode-type (:k subnode-type)
                        :node-type (g/node-type-kw basis resource-node-id)
                        :proj-path (resource/resource->proj-path (resource-node/as-resource-original basis resource-node-id))})))))

(defn set-setting
  ([settings-resource-node-id setting-path value]
   (g/with-auto-evaluation-context evaluation-context
     (set-setting settings-resource-node-id setting-path value evaluation-context)))
  ([settings-resource-node-id setting-path value evaluation-context]
   (let [form-data (test-support/valid-node-value settings-resource-node-id :form-data evaluation-context)
         meta-settings (:meta-settings form-data)
         user-data (:user-data (:form-ops form-data))]
     (if (or (some #(= setting-path (:path %)) meta-settings))
       (settings/set-tx-data user-data setting-path value)
       (throw (ex-info "Invalid setting path."
                       {:setting-path setting-path
                        :candidates (into (sorted-set)
                                          (map :path)
                                          meta-settings)}))))))

(defn set-setting!
  ([settings-resource-node-id setting-path value]
   (g/with-auto-evaluation-context evaluation-context
     (set-setting! settings-resource-node-id setting-path value evaluation-context)))
  ([settings-resource-node-id setting-path value evaluation-context]
   (g/transact
     (set-setting settings-resource-node-id setting-path value evaluation-context))))

(defn save-project! [project]
  (let [save-data (project/dirty-save-data project)]
    (project/write-save-data-to-disk! save-data nil)
    (let [workspace (project/workspace project)
          post-save-actions (disk/make-post-save-actions save-data)]
      (disk/process-post-save-actions! workspace post-save-actions))))
