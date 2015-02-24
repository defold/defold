(ns internal.clojure
  (:require [clojure.java.io :as io]
            [clojure.tools.namespace.file :refer [read-file-ns-decl]]
            [plumbing.core :refer [defnk]]
            [dynamo.file :as file]
            [dynamo.node :as n]
            [dynamo.system :as ds]
            [dynamo.types :as t]
            [service.log :as log])
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
        (ds/set-property node :namespace (UnloadableNamespace. ns-decl)))
      (catch clojure.lang.Compiler$CompilerException compile-error
        (println compile-error)))))

(n/defnode ClojureSourceNode
  (inherits n/ResourceNode)

  (property namespace UnloadableNamespace)

  (on :load
    (compile-source-node self (:project event) (:filename self))))
