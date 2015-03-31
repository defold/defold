(ns editor.workspace
  "Define the concept of a project, and its Project node type. This namespace bridges between Eclipse's workbench and
ordinary paths."
  (:require [clojure.java.io :as io]
            [dynamo.file :as file]
            [dynamo.graph :as g]
            [dynamo.node :as n]
            [dynamo.property :as dp]
            [dynamo.selection :as selection]
            [dynamo.system :as ds]
            [dynamo.types :as t]
            [dynamo.ui :as ui]
            [dynamo.util :refer :all]
            [internal.clojure :as clojure]
            [internal.ui.dialogs :as dialogs]
            [service.log :as log])
  (:import [java.io File FilterOutputStream]))

(defprotocol Resource
  (resource-type [this])
  (source-type [this])
  (read-only? [this])
  (path [this])
  (url [this])
  (resource-name [this]))

(def wrap-stream)

(defn- split-ext [^File f]
  (let [n ^String (.getPath f)
        i (.lastIndexOf n ".")]
    (if (pos? i)
      [(subs n 0 i) (subs n (inc i))]
      [n nil])))

(defn- relative-path [^File f1 ^File f2]
  (.toString (.relativize (.toPath f1) (.toPath f2))))

(defrecord FileResource [workspace ^File file children]
  Resource
  (resource-type [this] (get (:resource-types workspace) (second (split-ext file))))
  (source-type [this] (if (.isFile file) :file :folder))
  (read-only? [this] (.canRead file))
  (path [this] (relative-path (File. (:root workspace)) file))
  (url [this] (str "file:/" (relative-path (File. (:root workspace)) file)))
  (resource-name [this] (.getName file))

  io/IOFactory
  (io/make-input-stream  [this opts] (io/make-input-stream file opts))
  (io/make-reader        [this opts] (io/make-reader (io/make-input-stream this opts) opts))
  (io/make-output-stream [this opts] (wrap-stream workspace (io/make-output-stream file opts) file))
  (io/make-writer        [this opts] (io/make-writer (io/make-output-stream this opts) opts)))

(defmethod print-method FileResource [file-resource ^java.io.Writer w]
  (.write w (format "FileResource{:file %s :children %s}" (:file file-resource) (str (:children file-resource)))))

(defn- create-resource-tree [workspace ^File file]
  (let [children (if (.isFile file) [] (mapv #(create-resource-tree workspace %) (.listFiles file)))]
    (FileResource. workspace file children)))

(g/defnk produce-root [self root]
  (create-resource-tree self (File. root)))

(g/defnk produce-resource-list [self root]
  (let [root-node (create-resource-tree self (File. root))]
    (tree-seq #(= :folder (source-type %1)) :children root-node)))

(defn register-resource-type [workspace & {:keys [ext node-type load-fn setup-rendering-fn]}]
  (let [resource-type {:node-type node-type
                       :load-fn load-fn
                       :setup-rendering-fn setup-rendering-fn}
        resource-types (if (string? ext)
                         [(assoc resource-type :ext ext)]
                         (map (fn [ext] (assoc resource-type :ext ext)) ext))]
    (doseq [resource-type resource-types]
      (g/update-property workspace :resource-types assoc (:ext resource-type) resource-type))))

(defn get-resource-type [workspace ext]
  (get (:resource-types workspace) ext))

(g/defnode Workspace
  (property root t/Str)
  (property opened-files t/Any (default (atom #{})))
  (property resource-types t/Any)
  
  (output resource-list t/Any produce-resource-list)
  (output resource-tree FileResource produce-root)
  (output resource-types t/Any (g/fnk [resource-types] resource-types)))

(defn- wrap-stream [workspace stream file]
  (swap! (:opened-files workspace)
        (fn [rs]
          (when (get rs file)
            (throw (Exception. (format "File %s already opened" file))))
          (conj rs file)))
  (proxy [FilterOutputStream] [stream]
    (close []
      (swap! (:opened-files workspace) disj file)
      (proxy-super close))))