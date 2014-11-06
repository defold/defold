(ns internal.clojure
  (:require [clojure.java.io :as io]
            [clojure.tools.namespace.file :refer [read-file-ns-decl]]
            [plumbing.core :refer [defnk]]
            [dynamo.file :as file]
            [dynamo.node :refer [defnode]]
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

(defnk compile-clojure
  [this resource]
  (let [ns-decl     (read-file-ns-decl resource)
        source-file (file/eclipse-file resource)
        project     (iq/node-consuming this :self)]
    (markers/remove-markers source-file)
    (try
      (ds/transactional (:world-ref this)
        (ds/in project
         (Compiler/load (io/reader resource) (file/local-path resource) (.getName source-file))
         (UnloadableNamespace. ns-decl)))
      (catch clojure.lang.Compiler$CompilerException compile-error
        (markers/compile-error source-file compile-error)
        {:compile-error (.getMessage (.getCause compile-error))}))))

(defnode ClojureSourceNode
  (property resource IFile)
  (output   namespace UnloadableNamespace :cached :on-update compile-clojure))

(defn on-load-code
  [resource _]
  (ds/add (make-clojure-source-node :resource resource)))
