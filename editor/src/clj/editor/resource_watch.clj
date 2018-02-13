(ns editor.resource-watch
  (:require [clojure.java.io :as io]
            [clojure.string :as str]
            [editor.settings-core :as settings-core]
            [editor.library :as library]
            [editor.resource :as resource]
            [editor.system :as system]
            [dynamo.graph :as g])
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

(defn- extract-game-project-include-dirs [game-project-resource]
  (with-open [reader (io/reader game-project-resource)]
    (let [settings (settings-core/parse-settings reader)]
      (parse-include-dirs (str (settings-core/get-setting settings ["library" "include_dirs"]))))))

(defn- load-library-zip [workspace file]
  (let [base-path (library/library-base-path file)
        zip-resources (resource/load-zip-resources workspace file base-path)
        game-project-resource (first (filter (fn [resource] (= "game.project" (resource/resource-name resource))) (:tree zip-resources)))]
    (when game-project-resource
      (let [include-dirs (set (extract-game-project-include-dirs game-project-resource))]
        (update zip-resources :tree (fn [tree] (filter #(include-dirs (resource-root-dir %)) tree)))))))

(defn- make-library-snapshot [workspace lib-state]
  (let [file ^File (:file lib-state)
        tag (:tag lib-state)
        zip-file-version (if-not (str/blank? tag) tag (str (.lastModified file)))
        {resources :tree crc :crc} (load-library-zip workspace file)
        flat-resources (resource/resource-list-seq resources)]
    {:resources resources
     :status-map (into {} (map (fn [resource]
                                 (let [path (resource/proj-path resource)
                                       version (str zip-file-version ":" (crc path))]
                                   [path {:version version :source :library :library (:url lib-state)}]))
                               flat-resources))}))

(defn- update-library-snapshot-cache
  [library-snapshot-cache workspace lib-states]
  (reduce (fn [ret {:keys [^File file] :as lib-state}]
            (if file
              (let [lib-file-path (.getPath file)
                    cached-snapshot (get library-snapshot-cache lib-file-path)
                    mtime (.lastModified file)
                    snapshot (if (and cached-snapshot (= mtime (-> cached-snapshot meta :mtime)))
                               cached-snapshot
                               (with-meta (make-library-snapshot workspace lib-state)
                                 {:mtime mtime}))]
                (assoc ret lib-file-path snapshot))
              ret))
          {}
          lib-states))

(defn- make-library-snapshots [library-snapshot-cache lib-states]
  (into [] (comp
             (map :file)
             (filter some?)
             (map #(.getPath ^File %))
             (map library-snapshot-cache))
        lib-states))

(defn- make-builtins-snapshot [workspace]
  (let [resources (:tree (resource/load-zip-resources workspace (io/resource "builtins.zip")))
        flat-resources (resource/resource-list-seq resources)]
    {:resources resources
     :status-map (into {} (map (juxt resource/proj-path (constantly {:version :constant :source :builtins})) flat-resources))}))

(def reserved-proj-paths #{"/builtins" "/build" "/.internal" "/.git"})

(defn reserved-proj-path? [path]
  (reserved-proj-paths path))

(defn- file-resource-filter [^File root ^File f]
  (not (or (= (.charAt (.getName f) 0) \.)
           (reserved-proj-path? (resource/file->proj-path root f)))))

(defn- make-file-tree
  ([workspace ^File file]
   (make-file-tree workspace (io/file (g/node-value workspace :root)) file))
  ([workspace ^File root ^File file]
   (let [children (into []
                        (comp
                          (filter (partial file-resource-filter root))
                          (map #(make-file-tree workspace root %)))
                        (.listFiles file))]
     (resource/make-file-resource workspace (.getPath root) file children))))

(defn- file-resource-status [r]
  {:version (str (.lastModified ^File (io/file r))) :source :directory})

(defn- file-resource-status-map-entry [r]
  [(resource/proj-path r)
   (file-resource-status r)])

(defn- make-directory-snapshot [workspace ^File root]
  (assert (and root (.isDirectory root)))
  (let [resources (resource/children (make-file-tree workspace root))
        flat-resources (resource/resource-list-seq resources)]
    {:resources resources
     :status-map (into {} (map file-resource-status-map-entry) flat-resources)}))

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

(defn- make-debugger-snapshot
  [workspace]
  (let [base-path (if (system/defold-dev?)
                    ;; Use local debugger support files so we can see
                    ;; changes to them instantly without re-packing/restarting.
                    (.getAbsolutePath (io/file "bundle-resources"))
                    (system/defold-unpack-path))
        root (io/file base-path "_defold/debugger")
        mount-root (io/file base-path)
        resources (resource/children (make-file-tree workspace mount-root root))
        flat-resources (resource/resource-list-seq resources)]
    {:resources resources
     :status-map (into {} (map file-resource-status-map-entry) flat-resources)}))

(defn update-snapshot-status [snapshot resources]
  (assert (every? resource/file-resource? resources))
  (update snapshot :status-map
          (fn [status-map]
            (reduce (fn [status-map resource]
                      (assoc status-map (resource/proj-path resource) (file-resource-status resource)))
                    status-map
                    resources))))

(defn make-snapshot-info [workspace project-directory library-urls snapshot-cache]
  (let [lib-states (library/current-library-state project-directory library-urls)
        new-library-snapshot-cache (update-library-snapshot-cache snapshot-cache workspace lib-states)]
    {:snapshot (combine-snapshots (list* (make-builtins-snapshot workspace)
                                         (make-directory-snapshot workspace project-directory)
                                         (make-debugger-snapshot workspace)
                                         (make-library-snapshots new-library-snapshot-cache lib-states)))
     :snapshot-cache new-library-snapshot-cache}))

(defn make-resource-map [snapshot]
  (into {} (map (juxt resource/proj-path identity) (resource/resource-list-seq (:resources snapshot)))))

(defn- resource-status [snapshot path]
  (get-in snapshot [:status-map path]))

(defn diff [old-snapshot new-snapshot]
  (let [old-map (make-resource-map old-snapshot)
        new-map (make-resource-map new-snapshot)
        old-paths (set (keys old-map))
        new-paths (set (keys new-map))
        common-paths (clojure.set/intersection new-paths old-paths)
        added-paths (clojure.set/difference new-paths old-paths)
        removed-paths (clojure.set/difference old-paths new-paths)
        changed-paths (filter #(not= (resource-status old-snapshot %) (resource-status new-snapshot %)) common-paths)
        added (map new-map added-paths)
        removed (map old-map removed-paths)
        changed (map new-map changed-paths)]
    (assert (empty? (clojure.set/intersection (set added) (set removed))))
    (assert (empty? (clojure.set/intersection (set added) (set changed))))
    (assert (empty? (clojure.set/intersection (set removed) (set changed))))
    {:added added
     :removed removed
     :changed changed}))

(defn empty-diff? [diff]
  (not (or (seq (:added diff))
           (seq (:removed diff))
           (seq (:changed diff)))))
