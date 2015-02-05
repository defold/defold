(ns eclipse.resources
  (:require [clojure.core.async :refer [put!]]
            [dynamo.node :as n]
            [dynamo.system :as ds]
            [dynamo.types :as t]
            [eclipse.markers :as markers])
  (:import [org.eclipse.core.resources IContainer IFolder IResource IResourceChangeEvent IResourceChangeListener IResourceDelta IResourceDeltaVisitor IWorkspace ResourcesPlugin WorkspaceJob]))

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

(defn schedule-workspace-job
  "Defer execution of the thunk until after the current ResourceChangeEvent is done processing.
This avoids the 'Resource tree is locked' exception that can occur if you change resources while
an event is in flight."
  [job-name message-if-error scheduling-rule f & args]
  (doto
    (proxy [WorkspaceJob] [job-name]
          (runInWorkspace [monitor]
            (try
              (apply f args)
              markers/ok-status
              (catch Throwable t
                (markers/error-status message-if-error 0 t)))))
    (.setRule scheduling-rule)
    (.schedule)))

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
            (n/dispatch-message (ds/node world-ref project-node-id) :destroy))))
      IResourceChangeEvent/PRE_DELETE)))

(def ^:private delta-kinds
  {1 :added
   2 :deleted
   4 :changed})

(def ^:private delta-flags-of-interest
  (+ IResourceDelta/CONTENT IResourceDelta/REPLACED))

(defn- interesting?
  [^IResourceDelta delta]
  (and
    (not (instance? IContainer (.getResource delta)))
    (not (.. delta getFullPath (removeFirstSegments 1) toOSString (startsWith "content/builtins")))
    (< 2 (.. delta getFullPath segmentCount))
    (or
      (= IResourceDelta/REMOVED (.getKind delta))
      (= 0 (.getFlags delta))
      (not= 0 (bit-and (.getFlags delta) delta-flags-of-interest)))))

(defn- rce->map
  [^IResourceChangeEvent event]
  (let [deltas (atom {})
        visitor (reify IResourceDeltaVisitor
                  (^boolean visit [this ^IResourceDelta delta]
                    (when (interesting? delta)
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
                      (when (some #{(.getType event)} [IResourceChangeEvent/POST_CHANGE IResourceChangeEvent/PRE_DELETE])
                        (let [m (rce->map event)]
                          (when-not (empty? m)
                            (schedule-workspace-job "Refresh resources" "Error refreshing resources" (.getResource event) f m))))))]
    (.addResourceChangeListener workspace l)
    (reify t/IDisposable
         (dispose [this] (.removeResourceChangeListener workspace l)))))
