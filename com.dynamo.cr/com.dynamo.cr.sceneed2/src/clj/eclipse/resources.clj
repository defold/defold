(ns eclipse.resources
  (:require [clojure.core.async :refer [put!]]
            [dynamo.node :as n]
            [dynamo.types :as t]
            [internal.query :as iq])
  (:import [org.eclipse.core.resources IContainer IFolder IResource IResourceChangeEvent IResourceChangeListener IResourceDelta IResourceDeltaVisitor IWorkspace ResourcesPlugin]))

(set! *warn-on-reflection* true)

(defn resource-seq
  "Returns a lazy sequence of all the members in this container,
   recursively and depth-first."
  [container]
  (tree-seq (fn [^IResource m] (instance? IContainer m))
            (fn [^IContainer m] (.members m))
            container))

(defn ^IWorkspace workspace
  []
  (ResourcesPlugin/getWorkspace))

(defn listen-for-close
  [project-node & disposables]
  (let [project         (:eclipse-project project-node)
        project-node-id (:_id project-node)
        world-ref       (:world-ref project-node)]
    (.addResourceChangeListener (workspace)
      (reify IResourceChangeListener
        (resourceChanged [this event]
          (when (= (.getResource event) project)
            (doseq [d disposables] (t/dispose d))
            (n/dispatch-message (iq/node-by-id world-ref project-node-id) :destroy))))
      IResourceChangeEvent/PRE_DELETE)))

(def ^:private delta-kinds
  {1 :added
   2 :deleted
   4 :changed})

(defn- rce->map
  [^IResourceChangeEvent event]
  (let [deltas (atom {})
        visitor (reify IResourceDeltaVisitor
                  (^boolean visit [this ^IResourceDelta delta]
                    (when (and
                              (< 2 (.. delta getFullPath segmentCount))
                              (not (.. delta getFullPath (removeFirstSegments 1) toOSString (startsWith "content/builtins")))
                              (not (instance? IContainer (.getResource delta))))
                      ;; ignore spurious change events on the builtins folder
                      (swap! deltas update-in [(delta-kinds (.getKind delta))] conj (.. delta getFullPath (removeFirstSegments 2))))
                    (instance? IContainer (.getResource delta))))]
    (.accept (.getDelta event) visitor)
    @deltas))

(defn listen-for-change
  [f]
  (let [workspace (workspace)
        l         (reify
                    IResourceChangeListener
                    (resourceChanged [_ event]
                      (let [m (rce->map event)]
                        (when-not (empty? m)
                          (f m)))))]
    (.addResourceChangeListener workspace l)
    (reify t/IDisposable
         (dispose [this] (.removeResourceChangeListener workspace l)))))
