(ns eclipse.resources
  (:require [dynamo.node :as n]
            [internal.query :as iq])
  (:import [org.eclipse.core.resources IContainer IResource IResourceChangeEvent IResourceChangeListener IWorkspace ResourcesPlugin]))

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
  [project-node]
  (let [project         (:eclipse-project project-node)
        project-node-id (:_id project-node)
        world-ref       (:world-ref project-node)]
    (.addResourceChangeListener (workspace)
      (reify IResourceChangeListener
        (resourceChanged [this event]
          (when (= (.getResource event) project)
            (n/dispatch-message (iq/node-by-id world-ref project-node-id) :destroy))))
      IResourceChangeEvent/PRE_DELETE)))