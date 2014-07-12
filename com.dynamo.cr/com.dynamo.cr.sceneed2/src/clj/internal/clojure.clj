(ns internal.clojure
  (:require [clojure.java.io :as io]
            [clojure.tools.namespace.file :refer [read-file-ns-decl]]
            [dynamo.resource :as resource]
            [dynamo.file :as file]
            [dynamo.node :refer [defnode]]
            [plumbing.core :refer [defnk]]
            [eclipse.markers :as markers])
  (:import [org.eclipse.core.resources IFile]
           [clojure.lang LineNumberingPushbackReader]))

(defrecord UnloadableNamespace [ns-decl]
  resource/IDisposable
  (dispose [this]
    (when (list? ns-decl)
      (remove-ns (second ns-decl)))))

(defmacro within-ns
  [ns & body]
  `(let [old-ns# (ns-name *ns*)]
     (try
       (in-ns ~ns)
       ~@body
       (finally
         (in-ns old-ns#)))))

(defn- load-next [there rdr path name]
  (try
    (prn (var-get #'clojure.core/*ns*))
    (within-ns there
      (Compiler/load rdr path name))
    (catch clojure.lang.Compiler$CompilerException compile-error
      compile-error)))

(defnk load-project-file
  [project this g]
  (let [source      (:resource this)
        ns-decl     (read-file-ns-decl source)
        there       (if (= 'ns (first ns-decl)) (second ns-decl) 'user)
        source-file (file/eclipse-file source)]
    (markers/remove-markers source-file)
    (let [rdr (LineNumberingPushbackReader. (io/reader source))]
      (loop [res (load-next there rdr (file/local-path source) (.getName source-file))]
        (when (instance? Throwable res)
          (markers/compile-error source-file res)
          (recur (load-next there rdr (file/local-path source) (.getName source-file))))))
    (UnloadableNamespace. ns-decl)))

(def ClojureSourceFile
  {:properties {:resource {:schema IFile}}
   :transforms {:namespace #'load-project-file}
   :cached     #{:namespace}
   :on-update  #{:namespace}})

(defnode ClojureSourceNode
 ClojureSourceFile)
