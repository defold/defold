(ns editor.watch
  (:require [clojure.java.io :as io]
            [clojure.pprint :refer [pprint]]
            [clojure.core.async :as a])
  (:import [java.io File]))

(defrecord Watcher [root state changes])

(defn- file-map-seq [^File root]
  "Similar to file-seq but returns a map {file last-modified} with files only"
  (->> (file-seq root)
    (filter #(.isFile ^File %))
    (mapcat (fn [^File file] [file (.lastModified file)]))
    (apply hash-map)))

(defn- watch-filter [f state]
  (->> state
    (filter f)
    (mapv first)))

(defn- state-changes
  "Compare two states. Returns a map of with :added :removed :changed as sets"
  [current-state new-state]
  { :added (watch-filter (fn [[f t]] (not (current-state f))) new-state)
    :removed (watch-filter (fn [[f t]] (not (new-state f))) current-state)
    :changed (watch-filter (fn [[f t]] (and (new-state f) (not= (new-state f) t))) current-state)})

(defn make-watcher
  "create a new watcher. changes are stored in :changes. see update"
  [root]
  (Watcher. root (file-map-seq (io/file root)) {} ))

(defn update
  "update watcher. on file-system changes :changes contain sets for :added, :removed and :changed files"
  [watcher]
  (let [new-state (file-map-seq (io/file (:root watcher)))
        changes (state-changes (:state watcher) new-state)]
    (assoc watcher 
           :state new-state
           :changes changes)))

(defmethod print-method Watcher [v ^java.io.Writer w]
  (.write w (format "changes %s" (:changes v))))
