(ns internal.clojure
  (:require [clojure.java.io :as io]
            [clojure.tools.namespace.file :refer [read-file-ns-decl]]
            [dynamo.resource :as resource]
            [dynamo.file :as file]
            [dynamo.node :refer [defnode]]
            [dynamo.env :refer [*current-project*]]
            [plumbing.core :refer [defnk]]
            [eclipse.markers :as markers]
            [service.log :as log])
  (:import [org.eclipse.core.resources IFile]
           [clojure.lang LineNumberingPushbackReader]))

(defrecord UnloadableNamespace [ns-decl]
  resource/IDisposable
  (dispose [this]
    (when (list? ns-decl)
      (remove-ns (second ns-decl)))))

(defnk load-project-file
  [project this g]
  (let [source      (:resource this)
        ns-decl     (read-file-ns-decl source)
        source-file (file/eclipse-file source)]
    (markers/remove-markers source-file)
    (try
      (do
        (binding [*current-project* project]
          (Compiler/load (io/reader source) (file/local-path source) (.getName source-file))
          (UnloadableNamespace. ns-decl)))
      (catch clojure.lang.Compiler$CompilerException compile-error
        (markers/compile-error source-file compile-error)
        {:compile-error (.getMessage (.getCause compile-error))}))))

(def
  ^{:doc "Behavior included in `ClojureSourceNode`."}
  ClojureSourceFile
  {:properties {:resource {:schema IFile}}
   :transforms {:namespace #'load-project-file}
   :cached     #{:namespace}
   :on-update  #{:namespace}})

(defnode ClojureSourceNode
 ClojureSourceFile)
