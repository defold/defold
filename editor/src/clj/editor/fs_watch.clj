(ns editor.fs-watch
  (:require [clojure.java.io :as io])
  (:import [java.io File]))

(defrecord Watcher [root file-filter state changes])

(defn- file-map-seq [^File root file-filter]
  "Similar to file-seq but returns a map {file last-modified} with files only"
  (->> (tree-seq (fn [^File f] (and (.isDirectory f) (file-filter f))) (fn [^File f] (seq (.listFiles f))) root)
    (filter (fn [^File f] (file-filter f)))
    (mapcat (fn [^File file] [file (.lastModified file)]))
    (apply hash-map)))

(defn- watch-filter [f state]
  (->> state
    (filter f)
    (mapv first)))

(defn- state-changes
  "Compare two states. Returns a map with keys :added, :removed and :changed as sets"
  [current-state new-state]
  {:added (watch-filter (fn [[f t]] (not (current-state f))) new-state)
   :removed (watch-filter (fn [[f t]] (not (new-state f))) current-state)
   :changed (watch-filter (fn [[f t]] (and (new-state f) (not= (new-state f) t))) current-state)})

(defn make-watcher
  "Create a new watcher. changes are stored in :changes. see update"
  [root file-filter]
  (Watcher. root file-filter (file-map-seq (io/file root) file-filter) {} ))

(defn watch
  "Update watcher. On file-system changes :changes contains sets for :added, :removed and :changed files"
  [watcher]
  (let [new-state (file-map-seq (io/file (:root watcher)) (:file-filter watcher))
        changes (state-changes (:state watcher) new-state)]
    (assoc watcher
           :state new-state
           :changes changes)))

(defmethod print-method Watcher [v ^java.io.Writer w]
  (.write w (format "changes %s" (:changes v))))
