(ns internal.clojure
  (:require [clojure.java.io :as io]
            [clojure.tools.namespace.file :refer [read-file-ns-decl]]
            [dynamo.file :as file]
            [dynamo.graph :as g]
            [dynamo.node :as n]
            [dynamo.system :as ds]
            [dynamo.types :as t])
  (:import [clojure.lang LineNumberingPushbackReader]))

(defn clojure-source?
  [f]
  (and (.isFile f)
       (.endsWith (.getName f) ".clj")))

(defrecord UnloadableNamespace [ns-decl]
  t/IDisposable
  (dispose [this]
    (when (list? ns-decl)
      (remove-ns (second ns-decl)))))

(defn compile-source-node
  [node project path]
  (let [ns-decl     (read-file-ns-decl path)
        source-file (file/project-file path)]
    (try
      (ds/in project
        (binding [*warn-on-reflection* true]
          (Compiler/load (io/reader path) (t/local-path path) (.getName source-file)))
        (g/set-property node :namespace (UnloadableNamespace. ns-decl)))
      (catch clojure.lang.Compiler$CompilerException compile-error
        (println compile-error)))))

(g/defnode ClojureSourceNode
  (inherits g/ResourceNode)

  (property namespace UnloadableNamespace)

  (on :load
    (compile-source-node self (:project event) (:filename self))))
