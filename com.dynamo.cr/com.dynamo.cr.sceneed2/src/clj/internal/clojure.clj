(ns internal.clojure
  (:require [clojure.java.io :as io]
            [clojure.tools.namespace.file :refer [read-file-ns-decl]]
            [plumbing.core :refer [defnk]]
            [dynamo.file :as file]
            [dynamo.node :as n]
            [dynamo.system :as ds]
            [dynamo.types :as t]
            [internal.query :as iq]
            [eclipse.markers :as markers]
            [service.log :as log])
  (:import [org.eclipse.core.resources IFile IResource]
           [clojure.lang LineNumberingPushbackReader]))

(defn clojure-source?
  [^IResource resource]
  (and (instance? IFile resource)
       (.endsWith (.getName resource) ".clj")))

(defrecord UnloadableNamespace [ns-decl]
  t/IDisposable
  (dispose [this]
    (when (list? ns-decl)
      (remove-ns (second ns-decl)))))

(defn compile-source-node
  [node project path]
  (let [ns-decl     (read-file-ns-decl path)
        source-file (file/project-file path)]
    (markers/remove-markers source-file)
    (try
      (ds/in project
        (Compiler/load (io/reader path) (t/local-path path) (.getName source-file))
        (ds/set-property node :namespace (UnloadableNamespace. ns-decl)))
      (catch clojure.lang.Compiler$CompilerException compile-error
        (markers/compile-error source-file compile-error)
        {:compile-error (.getMessage (.getCause compile-error))}))))


(n/defnode ClojureSourceNode
  (inherits n/ResourceNode)

  (property namespace UnloadableNamespace)

  (on :load
    (compile-source-node self (:project event) (:filename self))))

