(ns editor.clojure
  (:require [clojure.java.io :as io]
            [clojure.tools.namespace.file :refer [read-file-ns-decl]]
            [dynamo.graph :as g]
            [editor.core :as core]
            [editor.file :as file]
            [editor.types :as types])
  (:import [clojure.lang LineNumberingPushbackReader]
           [java.io File]))

;; TODO - move this into editor

(defn clojure-source?
  [^File f]
  (and (.isFile f)
       (.endsWith (.getName f) ".clj")))

(defrecord UnloadableNamespace [ns-decl])

(defn compile-source-node
  [node project path]
  (let [ns-decl     (read-file-ns-decl path)
        ^File source-file (file/project-file path)]
    (try
      (binding [*warn-on-reflection* true]
        (Compiler/load (io/reader path) (types/local-path path) (.getName source-file)))
      (g/set-property (g/node-id node) :namespace (UnloadableNamespace. ns-decl))
      (catch clojure.lang.Compiler$CompilerException compile-error
        (println compile-error)))))

(g/defnode ClojureSourceNode
  (inherits core/ResourceNode)

  (property namespace UnloadableNamespace))
