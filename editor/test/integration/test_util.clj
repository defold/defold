(ns integration.test-util
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.atlas :as atlas]
            [editor.camera-editor :as camera]
            [editor.collada-scene :as collada-scene]
            [editor.collection :as collection]
            [editor.collection-proxy :as collection-proxy]
            [editor.collision-object :as collision-object]
            [editor.cubemap :as cubemap]
            [editor.factory :as factory]
            [editor.game-object :as game-object]
            [editor.game-project :as game-project]
            [editor.graph-util :as gu]
            [editor.image :as image]
            [editor.label :as label]
            [editor.defold-project :as project]
            [editor.resource :as resource]
            [editor.scene :as scene]
            [editor.scene-selection :as scene-selection]
            [editor.sprite :as sprite]
            [editor.font :as font]
            [editor.protobuf-types :as protobuf-types]
            [editor.script :as script]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [editor.gl.shader :as shader]
            [editor.rig :as rig]
            [editor.tile-map :as tile-map]
            [editor.tile-source :as tile-source]
            [editor.sound :as sound]
            [editor.spine :as spine]
            [editor.particlefx :as particlefx]
            [editor.gui :as gui]
            [editor.json :as json]
            [editor.model :as model]
            [editor.material :as material]
            [editor.handler :as handler]
            [editor.display-profiles :as display-profiles]
            [util.http-server :as http-server]
            [util.thread-util :as thread-util])
  (:import [java.io File FilenameFilter FileInputStream ByteArrayOutputStream]
           [java.nio.file Files attribute.FileAttribute]
           [javax.imageio ImageIO]
           [javafx.scene.control Tab]
           [org.apache.commons.io FileUtils FilenameUtils IOUtils]
           [java.util.zip ZipOutputStream ZipEntry]))

(def project-path "test/resources/test_project")

(defn setup-workspace!
  ([graph]
    (setup-workspace! graph project-path))
  ([graph project-path]
    (let [workspace (workspace/make-workspace graph project-path)]
      (g/transact
       (concat
         (scene/register-view-types workspace)))
      (g/transact
       (concat
        (atlas/register-resource-types workspace)
        (camera/register-resource-types workspace)
        (collada-scene/register-resource-types workspace)
        (collection/register-resource-types workspace)
        (collection-proxy/register-resource-types workspace)
        (collision-object/register-resource-types workspace)
        (cubemap/register-resource-types workspace)
        (display-profiles/register-resource-types workspace)
        (factory/register-resource-types workspace)
        (font/register-resource-types workspace)
        (game-object/register-resource-types workspace)
        (game-project/register-resource-types workspace)
        (gui/register-resource-types workspace)
        (image/register-resource-types workspace)
        (json/register-resource-types workspace)
        (label/register-resource-types workspace)
        (material/register-resource-types workspace)
        (model/register-resource-types workspace)
        (particlefx/register-resource-types workspace)
        (protobuf-types/register-resource-types workspace)
        (rig/register-resource-types workspace)
        (script/register-resource-types workspace)
        (shader/register-resource-types workspace)
        (sound/register-resource-types workspace)
        (spine/register-resource-types workspace)
        (sprite/register-resource-types workspace)
        (tile-map/register-resource-types workspace)
        (tile-source/register-resource-types workspace)))
      (workspace/resource-sync! workspace)
      workspace)))

(defn setup-scratch-workspace! [graph project-path]
  (let [temp-project-path (-> (Files/createTempDirectory "test" (into-array FileAttribute []))
                              (.toFile)
                              (.getAbsolutePath))]
    (FileUtils/copyDirectory (io/file project-path) (io/file temp-project-path))
    (setup-workspace! graph temp-project-path)))

(defn setup-project!
  ([workspace]
   (let [proj-graph (g/make-graph! :history true :volatility 1)
         project (project/make-project proj-graph workspace)
         project (project/load-project project)]
     (g/reset-undo! proj-graph)
     project))
  ([workspace resources]
   (let [proj-graph (g/make-graph! :history true :volatility 1)
         project (project/make-project proj-graph workspace)
         project (project/load-project project resources)]
     (g/reset-undo! proj-graph)
     project)))


(defrecord FakeFileResource [workspace root ^File file children exists? read-only? content]
  resource/Resource
  (children [this] children)
  (ext [this] (FilenameUtils/getExtension (.getPath file)))
  (resource-type [this] (get (g/node-value workspace :resource-types) (resource/ext this)))
  (source-type [this] :file)
  (exists? [this] exists?)
  (read-only? [this] read-only?)
  (path [this] (if (= "" (.getName file)) "" (resource/relative-path (File. ^String root) file)))
  (abs-path [this] (.getAbsolutePath  file))
  (proj-path [this] (if (= "" (.getName file)) "" (str "/" (resource/path this))))
  (url [this] (resource/relative-path (File. ^String root) file))
  (resource-name [this] (.getName file))
  (workspace [this] workspace)
  (resource-hash [this] (hash (resource/proj-path this)))

  io/IOFactory
  (io/make-input-stream  [this opts] (io/make-input-stream content opts))
  (io/make-reader        [this opts] (io/make-reader (io/make-input-stream this opts) opts))
  (io/make-output-stream [this opts] (assert false "writing to not supported"))
  (io/make-writer        [this opts] (assert false "writing to not supported")))

(defn make-fake-file-resource
  ([workspace root file content]
   (make-fake-file-resource workspace root file content nil))
  ([workspace root file content {:keys [children exists? read-only?]
                                 :or {children nil
                                      exists? true
                                      read-only? false}
                                 :as opts}]
   (FakeFileResource. workspace root file children exists? read-only? content)))

(defn resource-node [project path]
  (project/get-resource-node project path))

(defn empty-selection? [app-view]
  (let [sel (g/node-value app-view :selected-node-ids)]
    (empty? sel)))

(defn selected? [app-view tgt-node-id]
  (let [sel (g/node-value app-view :selected-node-ids)]
    (not (nil? (some #{tgt-node-id} sel)))))

(g/defnode MockAppView
  (inherits app-view/AppView)
  (property mock-active-resource resource/Resource)
  (property mock-open-resources g/Any)
  (output active-resource resource/Resource (g/fnk [mock-active-resource] mock-active-resource))
  (output open-resources g/Any (g/fnk [mock-open-resources] mock-open-resources)))

(defn make-view-graph! []
  (g/make-graph! :history false :volatility 2))

(defn setup-app-view! [project]
  (let [view-graph (make-view-graph!)]
    (-> (g/make-nodes view-graph [app-view [MockAppView :active-tool :move]]
          (g/connect project :_node-id app-view :project-id)
          (for [label [:selected-node-ids-by-resource :selected-node-properties-by-resource :sub-selections-by-resource]]
            (g/connect project label app-view label)))
      g/transact
      g/tx-nodes-added
      first)))

(defn open-tab! [project app-view path]
  (let [node-id (project/get-resource-node project path)
        resource (g/node-value node-id :resource)
        open-resources (set (g/node-value app-view :open-resources))]
    (if (contains? open-resources resource)
      (g/set-property! app-view :mock-active-resource resource)
      (do
        (g/transact
          (concat
            (g/set-property app-view :mock-active-resource resource)
            (g/update-property app-view :mock-open-resources (comp vec conj) resource)))
        (app-view/select! app-view [node-id])))
    node-id))

(defn close-tab! [project app-view path]
  (let [node-id (project/get-resource-node project path)
        resource (g/node-value node-id :resource)]
    (g/transact
      (concat
        (g/update-property app-view :mock-active-resource (fn [res] (if (= res resource) nil res)))
        (g/update-property app-view :mock-open-resources (fn [resources] (filterv #(not= resource %) resources)))))))

(defn setup!
  ([graph]
    (setup! graph project-path))
  ([graph project-path]
    (let [workspace (setup-workspace! graph project-path)
          project   (setup-project! workspace)
          app-view  (setup-app-view! project)]
      [workspace project app-view])))

(defn set-active-tool! [app-view tool]
  (g/transact (g/set-property app-view :active-tool tool)))

(defn open-scene-view! [project app-view path width height]
  (let [resource-node (open-tab! project app-view path)
        view-graph (g/make-graph! :history false :volatility 2)
        view-id (scene/make-preview view-graph resource-node {:app-view app-view :project project} width height)]
    [resource-node view-id]))

(defn- fake-input!
  ([view type x y]
    (fake-input! view type x y []))
  ([view type x y modifiers]
    (fake-input! view type x y modifiers 0))
  ([view type x y modifiers click-count]
    (let [pos [x y 0.0]]
      (g/transact (g/set-property view :tool-picking-rect (scene-selection/calc-picking-rect pos pos))))
    (let [handlers  (g/sources-of view :input-handlers)
          user-data (g/node-value view :selected-tool-renderables)
          action    (reduce #(assoc %1 %2 true)
                            {:type type :x x :y y :click-count click-count}
                            modifiers)
          action    (scene/augment-action view action)]
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
  (let [image (g/node-value view :frame)]
    (let [file (File. path)]
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

(defn prop [node-id label]
  (get-in (g/node-value node-id :_properties) [:properties label :value]))

(defn prop-error [node-id label]
  (get-in (g/node-value node-id :_properties) [:properties label :error]))

(defn prop-node-id [node-id label]
  (get-in (g/node-value node-id :_properties) [:properties label :node-id]))

(defn prop! [node-id label val]
  (g/transact (g/set-property (prop-node-id node-id label) label val)))

(defn prop-clear! [node-id label]
  (g/transact (g/clear-property (prop-node-id node-id label) label)))

(defn prop-read-only? [node-id label]
  (get-in (g/node-value node-id :_properties) [:properties label :read-only?]))

(defn prop-overridden? [node-id label]
  (contains? (get-in (g/node-value node-id :_properties) [:properties label]) :original-value))

(defn resource [workspace path]
  (workspace/file-resource workspace path))

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
                                                files (->> (tree-seq (fn [^File f] (.isDirectory f)) (fn [^File f] (.listFiles f file-filter)) (File. (format "test/resources/%s" lib)))
                                                        (filter (fn [^File f] (not (.isDirectory f)))))]
                                            (with-open [byte-stream (ByteArrayOutputStream.)
                                                        out (ZipOutputStream. byte-stream)]
                                              (doseq [f files]
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

(defn lib-server-url [server lib]
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

(defn make-graph-reverter
  "Returns an AutoCloseable that reverts the specified graph to
  the state it was at construction time when its close method
  is invoked. Suitable for use with the (with-open) macro."
  [graph-id]
  (let [initial-undo-stack-count (g/undo-stack-count graph-id)]
    (reify java.lang.AutoCloseable
      (close [_]
        (loop [undo-stack-count (g/undo-stack-count graph-id)]
          (when (< initial-undo-stack-count undo-stack-count)
            (g/undo! graph-id)
            (recur (g/undo-stack-count graph-id))))))))

(defn add-embedded-component!
  "Adds a new instance of an embedded component to the specified
  game object node inside a transaction and makes it the current
  selection. Returns the id of the added EmbeddedComponent node."
  [app-view select-fn resource-type go-id]
  (game-object/add-embedded-component-handler {:_node-id go-id :resource-type resource-type} select-fn)
  (first (selection app-view)))

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
