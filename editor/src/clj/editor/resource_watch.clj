(ns editor.resource-watch
  (:require [clojure.java.io :as io]
            [clojure.string :as str]
            [editor.game-project-core :as game-project-core]
            [editor.library :as library]
            [editor.resource :as resource])
  (:import [java.io File]
           [editor.resource Resource FileResource ZipResource]))

(set! *warn-on-reflection* true)

(defn- resource-root-dir [resource]
  (when-let [path-splits (seq (rest (str/split (resource/proj-path resource) #"/")))] ; skip initial ""
    (if (= (count path-splits) 1)
      (when (= (resource/source-type resource) :folder)
        (first path-splits))
      (first path-splits))))

(defn parse-include-dirs [include-string]
  (filter (comp not str/blank?) (str/split include-string  #"[,\s]")))
  
(defn- extract-game-project-include-dirs [reader]
  (let [settings (game-project-core/parse-settings reader)]
    (parse-include-dirs (str (game-project-core/get-setting settings ["library" "include_dirs"])))))

(defn- make-library-zip-tree [workspace file]
  (let [zip-resources (resource/make-zip-tree workspace file)
        flat-zip-resources (resource/resource-list-seq zip-resources)
        game-project-resource (first (filter (fn [resource] (= "game.project" (resource/resource-name resource))) flat-zip-resources))]
    (when game-project-resource
      (let [include-dirs (set (extract-game-project-include-dirs (io/reader game-project-resource)))]
        (filter #(include-dirs (resource-root-dir %)) zip-resources)))))

(defn- make-library-snapshot [workspace lib-state]
  (let [file ^File (:file lib-state)
        tag (:tag lib-state)
        version (if-not (str/blank? tag) tag (str (.lastModified file)))
        resources (make-library-zip-tree workspace file)
        flat-resources (resource/resource-list-seq resources)]
    {:resources resources
     :status-map (into {} (map (juxt resource/proj-path (constantly {:version version :source :library :library (:url lib-state)})) flat-resources))}))

(defn- make-library-snapshots [workspace project-directory library-urls]
  (let [lib-states (filter :file (library/current-library-state project-directory library-urls))]
    (map (partial make-library-snapshot workspace) lib-states)))

(defn- make-builtins-snapshot [workspace]
  (let [resources (resource/make-zip-tree workspace (io/resource "builtins.zip"))
        flat-resources (resource/resource-list-seq resources)]
    {:resources resources
     :status-map (into {} (map (juxt resource/proj-path (constantly {:version :constant :source :builtins})) flat-resources))}))

(defn- file-resource-filter [^File f]
  (let [name (.getName f)]
    (not (or (= name "build") ; dont look in build/
             (= name "builtins") ; ?
             (= (subs name 0 1) "."))))) ; dont look at dot-files (covers .internal/lib)

(defn- make-file-tree [workspace ^File root]
  (let [children (if (.isFile root) [] (mapv #(make-file-tree workspace %) (filter file-resource-filter (.listFiles root))))]
    (resource/FileResource. workspace root children)))

(defn- file-resource-status-map-entry [r]
  [(resource/proj-path r)
   {:version (str (.lastModified ^File (:file r)))
    :source :directory}])

(defn- make-directory-snapshot [workspace ^File root]
  (assert (.isDirectory root))
  (let [resources (resource/children (make-file-tree workspace root))
        flat-resources (resource/resource-list-seq resources)]
    {:resources resources
     :status-map (into {} (map file-resource-status-map-entry flat-resources))}))

(defn- resource-paths [snapshot]
  (set (keys (:status-map snapshot))))

(defn empty-snapshot []
  {:resources nil
   :status-map nil
   :errors nil})

(defn- combine-snapshots [snapshots]
  (reduce
   (fn [result snapshot]
     (if-let [collisions (seq (clojure.set/intersection (resource-paths result) (resource-paths snapshot)))]
       (update result :errors conj {:collisions (select-keys (:status-map snapshot) collisions)})
       (-> result
           (update :resources concat (:resources snapshot))
           (update :status-map merge (:status-map snapshot)))))
   (empty-snapshot)
   snapshots))

(defn make-snapshot [workspace project-directory library-urls]
  (let [builtins-snapshot (make-builtins-snapshot workspace)
        fs-snapshot (make-directory-snapshot workspace project-directory)
        library-snapshots (make-library-snapshots workspace project-directory library-urls)]
    (combine-snapshots (list* builtins-snapshot fs-snapshot library-snapshots))))

(defn make-resource-map [snapshot]
  (into {} (map (juxt resource/proj-path identity) (resource/resource-list-seq (:resources snapshot)))))

(defn- resource-version [snapshot path]
  (get-in snapshot [:status-map path :version]))

(defn diff [old-snapshot new-snapshot]
  (let [old-map (make-resource-map old-snapshot)
        new-map (make-resource-map new-snapshot)
        old-paths (set (keys old-map))
        new-paths (set (keys new-map))
        common-paths (clojure.set/intersection new-paths old-paths)
        added-paths (clojure.set/difference new-paths old-paths)
        removed-paths (clojure.set/difference old-paths new-paths)
        changed-paths (filter #(not= (resource-version old-snapshot %) (resource-version new-snapshot %)) common-paths)]
    {:added (map new-map added-paths)
     :removed (map old-map removed-paths)
     :changed (map new-map changed-paths)}))

(defn empty-diff? [diff]
  (not (or (seq (:added diff))
           (seq (:removed diff))
           (seq (:changed diff)))))

