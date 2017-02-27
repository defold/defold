(ns editor.pipeline.bob
  (:require [clojure.java.io :as io]
    [dynamo.graph :as g]
    [editor.defold-project :as project]
    [editor.resource :as resource]
    [editor.ui :as ui]
    [editor.workspace :as workspace])
  (:import
    [com.dynamo.bob ClassLoaderScanner ClassLoaderResourceScanner CompileExceptionError IProgress IResourceScanner Project TaskResult]
    [com.dynamo.bob.fs DefaultFileSystem IResource]
    [com.dynamo.bob.util BobProjectProperties PathUtil]
    [java.io File]
    [java.net URL URLEncoder]))

(set! *warn-on-reflection* true)

(def skip-dirs #{".git" "build/default" ".internal"})
(def html5-url-prefix "/html5")

;; TODO - this should be fixed with proper progress at some point
(defn ->progress []
  (reify IProgress
    (subProgress [this work]
      (->progress))
    (worked [this amount]
      )
    (beginTask [this name work]
      )
    (done [this]
      )
    (isCanceled [this]
      false)))

(defn ->graph-resource-scanner [ws]
  (let [res-map (->> (g/node-value ws :resource-map)
                  (map (fn [[key val]] [(subs key 1) val]))
                  (into {}))]
    (reify IResourceScanner
      (getResource ^URL [this path]
        (when-let [r (get res-map path)]
          (io/as-url r)))
      (scan [this pattern]
        (let [res (->> res-map
                    (map first)
                    (filter #(PathUtil/wildcardMatch % pattern))
                    set)]
          res)))))

(defn- escaped-title [project]
  (let [proj-settings (project/settings project)
        title (get proj-settings ["project" "title"] "Unnamed")]
    (.replace (URLEncoder/encode title "UTF-8") "+" "%20")))
    
(defn- output-path [project]
  (let [ws (project/workspace project)
        build-path (workspace/build-path ws)]
    (str build-path "__htmlLaunchDir")))

(defn- task->error [project ^TaskResult r]
  (let [node-id (if-let [bob-resource ^IResource (first (.getInputs (.getTask r)))]
                  (let [path (str "/" (.getPath bob-resource))]
                    (project/get-resource-node project path))
                  0)]
    {:_node-id node-id
     :value (reify clojure.lang.IExceptionInfo
              (getData [this]
                {:line (.getLineNumber r)}))
     :message (.getMessage r)}))

(defn- exc->error [project ^CompileExceptionError e]
  (let [node-id (if-let [bob-resource ^IResource (.getResource e)]
                  (let [path (str "/" (.getPath bob-resource))]
                    (project/get-resource-node project path))
                  0)]
    {:_node-id node-id
     :value (reify clojure.lang.IExceptionInfo
              (getData [this]
                {:line (.getLineNumber e)}))
     :message (.getMessage e)}))

(defn build-html5 [project {:keys [clear-errors! render-error!] :as build-options}]
  (future
    (ui/run-later (clear-errors!))
    (let [progress (->progress)
          ws (project/workspace project)
          proj-path (str (workspace/project-path ws))
          output-path (output-path project)
          proj-settings (project/settings project)
          bob-args (cond-> {"archive" "true"
                            "platform" "js-web"
                            "bundle-output" output-path
                            "local-launch" "true"}
                     (get proj-settings ["project" "compress_archive"])
                     (assoc "compress" "true"))
          bob-project (Project. (DefaultFileSystem.) proj-path "build/default")
          commands (into-array String ["distclean" "build" "bundle"])]
      (doseq [[key val] bob-args]
        (.setOption bob-project key val))
      (.createPublisher bob-project (.hasOption bob-project "liveupdate"))
      (let [scanner (ClassLoaderScanner.)]
        (doseq [pkg ["com.dynamo.bob" "com.dynamo.bob.pipeline"]]
          (.scan bob-project scanner pkg)))
      (.mount bob-project (->graph-resource-scanner ws))
      (.findSources bob-project proj-path skip-dirs)
      (try
        (let [result (.build bob-project progress commands)
              failed-tasks (filter (fn [^TaskResult r] (not (.isOk r))) result)]
          (if (seq failed-tasks)
            (let [errors {:causes (map (partial task->error project) failed-tasks)}]
              (render-error! errors)
              false)
            true))
        (catch CompileExceptionError error
          (render-error! {:causes [(exc->error project error)]})
          false)))))

(defn- handler [project {:keys [url method headers]}]
  (if (= method "GET")
    (let [path (subs url (count html5-url-prefix))
          path (format "%s/%s%s" (output-path project) (escaped-title project) path)
          f (io/file path)]
      (if (.exists f)
        (let [length (.length f)
              response {:code 200
                        :response-headers {"Content-Length" (str length)}
                        :body f}]
          response)
        {:code 404}))))

(defn html5-handler [project req-headers]
  (handler project req-headers))
