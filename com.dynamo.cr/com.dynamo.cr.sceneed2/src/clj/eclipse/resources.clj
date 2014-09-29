(ns eclipse.resources
  (:import [org.eclipse.core.resources IContainer IResource]))

(set! *warn-on-reflection* true)

(defn resource-seq
  "Returns a lazy sequence of all the members in this container,
   recursively and depth-first."
  [container]
  (tree-seq (fn [^IResource m] (instance? IContainer m))
            (fn [^IContainer m] (.members m))
            container))
