(ns eclipse.resources
  (:import [org.eclipse.core.resources IContainer]))

(defn resource-seq
  "Returns a lazy sequence of all the members in this container,
   recursively and depth-first."
  [container]
  (tree-seq (fn [m] (instance? IContainer m))
            (fn [m] (.members m))
            container))
