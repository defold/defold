(ns editor.resources
  (:require [clojure.java.io :as io])
  (:import [java.io File FilterOutputStream]))

#_(defn tree-item-seq [item]
   (tree-seq
     #(not (.isLeaf %))
     #(seq (.getChildren %))
     item))

#_(defn- refresh-assets-browser [^TreeView tree content-root]
   (let [root (.getRoot tree)
         new-root (tree-item content-root)
         item-seq (tree-item-seq root)
         expanded (zipmap (map #(.getValue ^TreeItem %) item-seq)
                          (map #(.isExpanded ^TreeItem %) item-seq))]

     (doseq [^TreeItem item (tree-item-seq new-root)]
       (when (get expanded (.getValue item))
         (.setExpanded item true)))

     (.setRoot tree new-root)))

(def wrap-stream)

(defn- relative-path [^File f1 ^File f2]
  (.toString (.relativize (.toPath f1) (.toPath f2))))

(defprotocol Resource
  (resource-type [this])
  (read-only? [this])
  (path [this]))

(defrecord Resources [^File content-root opened-files])

(defrecord FileResource [resources ^File file children]
  Resource
  (resource-type [this] (if (.isFile file) :file :folder))
  (read-only? [this] (.canRead file))
  (path [this] (prn "XX" resources) "foo" #_(relative-path (:content-root resources) file))

  io/IOFactory
  (io/make-input-stream  [this opts] (io/make-input-stream file opts))
  (io/make-reader        [this opts] (io/make-reader (io/make-input-stream this opts) opts))
  (io/make-output-stream [this opts] (wrap-stream resources (io/make-output-stream file opts) file))
  (io/make-writer        [this opts] (io/make-writer (io/make-output-stream this opts) opts)))

(defmethod print-method FileResource [file-resource ^java.io.Writer w]
  (.write w (format "FileResource{:file %s :children %s}" (:file file-resource) (str (:children file-resource)))))

(defn make-resources [^File content-root]
  (Resources. content-root (atom #{})))

(defn- list-resources [resources ^File parent]
  (if (.isFile parent)
    (FileResource. resources parent [])
    (FileResource. resources parent (mapv #(list-resources resources %) (.listFiles parent)))))

(list-resources (make-resources (io/file "/tmp")) (io/file "/tmp/AlertDialog_css"))

(defn get-resource [resources path]
  (FileResource. resources (File. (:content-root resources) path) []))

(defn- wrap-stream [resources stream file]
  (swap! (:opened-files resources)
        (fn [rs]
          (when (get rs file)
            (throw (Exception. (format "File %s already opened" file))))
          (conj rs file)))
  (proxy [FilterOutputStream] [stream]
    (close []
      (swap! (:opened-files resources) disj file)
      (proxy-super close))))

(let [resources (make-resources (io/resource "/tmp"))
      r (get-resource resources "hej.txt")]
  (with-open [os (io/output-stream r)]
   (prn (:opened-files resources))))

;(slurp (FileResource. "/tmp/g.dot"))
