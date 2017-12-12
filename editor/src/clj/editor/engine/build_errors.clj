(ns editor.engine.build-errors
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [editor.defold-project :as project]
            [editor.resource :as resource]
            [dynamo.graph :as g])
  (:import (clojure.lang IExceptionInfo)
           (com.dynamo.bob CompileExceptionError MultipleCompileException MultipleCompileException$Info Task TaskResult)
           (com.dynamo.bob.bundle BundleHelper BundleHelper$ResourceInfo)
           (com.dynamo.bob.fs IResource)
           (java.util ArrayList)))

(set! *warn-on-reflection* true)

(def ^:private no-information-message "An error occurred while building Native Extensions, but the server provided no information.")

(defn- root-task ^Task [^Task task]
  (if-let [parent (.getProductOf task)]
    (recur parent)
    task))

(defprotocol ErrorInfoProvider
  (error-message [this] "Returns a non-empty string describing the issue.")
  (error-path [this] "Returns a slash-prefixed project-relative path to the offending resource, or nil if the issue does not relate to a particular resource.")
  (error-line [this] "Returns a 1-based index denoting the offending line in the resource. Can be nil or zero if the issue does not relate to a particular line.")
  (error-severity [this] "Returns either :info, :warning, or :fatal."))

(extend-type CompileExceptionError
  ErrorInfoProvider
  (error-message [this] (.getMessage this))
  (error-path [this] (some->> this .getResource .getPath (str "/")))
  (error-line [this] (.getLineNumber this))
  (error-severity [_this] :fatal))

(extend-type MultipleCompileException$Info
  ErrorInfoProvider
  (error-message [this] (.getMessage this))
  (error-path [this] (some->> this .getResource .getPath (str "/")))
  (error-line [this] (.getLineNumber this))
  (error-severity [this] (condp = (.getSeverity this)
                           MultipleCompileException$Info/SEVERITY_ERROR :fatal
                           MultipleCompileException$Info/SEVERITY_WARNING :warning
                           MultipleCompileException$Info/SEVERITY_INFO :info)))

(extend-type BundleHelper$ResourceInfo
  ErrorInfoProvider
  (error-message [this] (.message this))
  (error-path [this] (some->> this .resource (str "/")))
  (error-line [this] (.lineNumber this))
  (error-severity [this] (condp = (.severity this)
                           "error" :fatal
                           "warning" :warning
                           :info)))

(extend-type TaskResult
  ErrorInfoProvider
  (error-message [this] (if-let [message (.getMessage this)]
                          message
                          (throw (or (.getException this)
                                     (ex-info "TaskResult failed without message or exception." {})))))
  (error-path [this] (some->> this .getTask root-task .getInputs ^IResource first .getPath (str "/")))
  (error-line [this] (.getLineNumber this))
  (error-severity [_this] :fatal))

(defn- error-info-provider->cause [project e]
  (let [node-id (when-let [path (error-path e)]
                  (project/get-resource-node project path))
        line (error-line e)
        value (when (and (integer? line) (pos? line))
                (reify IExceptionInfo
                  (getData [_]
                    {:line (error-line e)})))
        message (error-message e)
        adjusted-message (if (string/ends-with? (string/lower-case message) "internal server error")
                           no-information-message
                           message)
        severity (error-severity e)]
    {:_node-id node-id
     :value value
     :message adjusted-message
     :severity severity}))

(def ^:private invalid-lib-re #"(?m)^Invalid lib in extension '(.+?)'.*$")
(def ^:private manifest-ext-name-re1 #"(?m)^name:\s*\"(.+)\"\s*$") ; Double-quoted name
(def ^:private manifest-ext-name-re2 #"(?m)^name:\s*'(.+)'\s*$") ; Single-quoted name

(defn- invalid-lib-match->cause [project [all manifest-ext-name] manifest-ext-resources-by-name]
  (assert (string? (not-empty all)))
  (assert (string? (not-empty manifest-ext-name)))
  (let [manifest-ext-resource (manifest-ext-resources-by-name manifest-ext-name)
        node-id (project/get-resource-node project manifest-ext-resource)]
    {:_node-id node-id
     :message all
     :severity :fatal}))

(defn- try-parse-manifest-ext-name [resource]
  (when (satisfies? io/IOFactory resource)
    (let [file-contents (slurp resource)]
      (second (or (re-find manifest-ext-name-re1 file-contents)
                  (re-find manifest-ext-name-re2 file-contents))))))

(defn- extension-manifest? [resource]
  (= "ext.manifest" (resource/resource-name resource)))

(defn- extension-manifests [project]
  (filterv extension-manifest? (g/node-value project :resources)))

(defn- extension-manifest-node-ids [project]
  (keep (partial project/get-resource-node project) (extension-manifests project)))

(defn- get-manifest-ext-resources-by-name [project]
  (into {}
        (keep (fn [resource]
                (when-let [manifest-ext-name (try-parse-manifest-ext-name resource)]
                  [manifest-ext-name resource])))
        (extension-manifests project)))

(defn- try-parse-invalid-lib-error-causes [project log]
  (when-let [matches (not-empty (re-seq invalid-lib-re log))]
    (let [manifest-ext-resources-by-name (get-manifest-ext-resources-by-name project)]
      (mapv #(invalid-lib-match->cause project % manifest-ext-resources-by-name) matches))))

(defn- try-parse-compiler-error-causes [project platform log]
  (let [issue-resource-infos (ArrayList.)]
    (BundleHelper/parseLog platform log issue-resource-infos)
    (not-empty (mapv (partial error-info-provider->cause project)
                     issue-resource-infos))))

(defn- try-get-multiple-compile-exception-error-causes [project ^MultipleCompileException exception]
  (not-empty (mapv (partial error-info-provider->cause project)
                   (.issues exception))))

(defn- generic-extension-error-causes [project log]
  ;; This is a catch-all that simply dumps the entire log output into the Build
  ;; Errors view in case we've failed to parse more meaningful errors from it.
  ;; It will split log lines across multiple build error entries, since the
  ;; control we use currently does not cope well with multi-line strings.
  (let [node-id (first (extension-manifest-node-ids project))]
    (or (when-let [log-lines (seq (drop-while string/blank? (string/split-lines (string/trim-newline log))))]
          (vec (map-indexed (fn [index log-line]
                              {:_node-id node-id
                               :message log-line
                               :severity (if (zero? index) :fatal :info)})
                            log-lines)))
        [{:_node-id node-id
          :message no-information-message
          :severity :fatal}])))

(defn failed-tasks-error-causes [project failed-tasks]
  (mapv (partial error-info-provider->cause project)
        failed-tasks))

(defn unsupported-platform-error [platform]
  (ex-info (str "Unsupported platform " platform)
           {:type ::unsupported-platform-error
            :platform platform}))

(defn unsupported-platform-error? [exception]
  (= ::unsupported-platform-error (:type (ex-data exception))))

(defn unsupported-platform-error-causes [project]
  [{:_node-id (first (extension-manifest-node-ids project))
    :message "Native Extensions are not yet supported for the target platform"
    :severity :fatal}])

(defn missing-resource-error [prop-name referenced-proj-path referencing-node-id]
  (ex-info (format "%s '%s' could not be found" prop-name referenced-proj-path)
           {:type ::missing-resource-error
            :node-id referencing-node-id
            :proj-path referenced-proj-path}))

(defn- missing-resource-error? [exception]
  (= ::missing-resource-error (:type (ex-data exception))))

(defn- missing-resource-error-causes [^Throwable exception]
  [{:_node-id (:node-id (ex-data exception))
    :message (.getMessage exception)
    :severity :fatal}])

(defn build-error [platform status log]
  (ex-info (format "Failed to build engine, status %d: %s" status log)
           {:type ::build-error
            :platform platform
            :status status
            :log log}))

(defn build-error? [exception]
  (= ::build-error (:type (ex-data exception))))

(defn build-error-causes [project exception]
  (let [log (:log (ex-data exception))
        platform (:platform (ex-data exception))]
    (or (try-parse-invalid-lib-error-causes project log)
        (try-parse-compiler-error-causes project platform log)
        (generic-extension-error-causes project log))))

(defn compile-exception-error-causes [project exception]
  [(error-info-provider->cause project exception)])

(defn multiple-compile-exception-error-causes [project ^MultipleCompileException exception]
  (let [log (.getRawLog exception)]
    (or (try-parse-invalid-lib-error-causes project log)
        (try-get-multiple-compile-exception-error-causes project exception)
        (generic-extension-error-causes project log))))

(defn handle-build-error! [render-error! project exception]
  (cond
    (unsupported-platform-error? exception)
    (do (render-error! {:causes (unsupported-platform-error-causes project)})
        true)

    (missing-resource-error? exception)
    (do (render-error! {:causes (missing-resource-error-causes exception)})
        true)

    (build-error? exception)
    (do (render-error! {:causes (build-error-causes project exception)})
        true)

    (instance? CompileExceptionError exception)
    (do (render-error! {:causes (compile-exception-error-causes project exception)})
        true)

    (instance? MultipleCompileException exception)
    (do (render-error! {:causes (multiple-compile-exception-error-causes project exception)})
        true)

    :else
    false))
