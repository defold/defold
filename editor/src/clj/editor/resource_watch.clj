;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.resource-watch
  (:require [clojure.java.io :as io]
            [clojure.set :as set]
            [clojure.string :as str]
            [dynamo.graph :as g]
            [editor.library :as library]
            [editor.resource :as resource]
            [editor.settings-core :as settings-core]
            [editor.system :as system]
            [util.coll :as coll :refer [pair]]
            [util.fn :as fn])
  (:import [java.io File]
           [java.net URI]))

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
        game-project-resource (some (fn [resource]
                                      (when (= "game.project" (resource/resource-name resource))
                                        resource))
                                    (:tree zip-resources))]
    (when game-project-resource
      (let [include-dirs (set (extract-game-project-include-dirs game-project-resource))]
        (update zip-resources :tree (fn [resources]
                                      (filterv #(include-dirs (resource-root-dir %))
                                               resources)))))))

(defn- make-library-snapshot [workspace lib-state]
  (let [file ^File (:file lib-state)
        tag (:tag lib-state)
        uri-string (.toString ^URI (:uri lib-state))
        zip-file-version (if-not (str/blank? tag) tag (str (.lastModified file)))
        {resources :tree crc :crc} (load-library-zip workspace file)]
    {:resources resources
     :status-map (into {}
                       (comp resource/xform-recursive-resources
                             (map (fn [resource]
                                    (let [proj-path (resource/proj-path resource)
                                          version (str zip-file-version ":" (crc proj-path))]
                                      (pair proj-path
                                            {:version version
                                             :source :library
                                             :library uri-string})))))
                       resources)}))

(defn- update-library-snapshot-cache
  [library-snapshot-cache workspace lib-states]
  (into library-snapshot-cache
        (keep (fn [lib-state]
                (when-some [^File file (:file lib-state)]
                  (let [lib-file-path (.getPath file)
                        mtime (.lastModified file)
                        cached-snapshot (get library-snapshot-cache lib-file-path)]
                    (when (or (nil? cached-snapshot)
                              (not= mtime (:mtime (meta cached-snapshot))))
                      (pair lib-file-path
                            (with-meta (make-library-snapshot workspace lib-state)
                                       {:mtime mtime})))))))
        lib-states))

(defn- make-library-snapshots [library-snapshot-cache lib-states]
  (into []
        (comp (keep :file)
              (map #(.getPath ^File %))
              (map library-snapshot-cache))
        lib-states))

(defn- make-builtins-snapshot-raw [workspace]
  (let [unpack-path (system/defold-unpack-path)
        builtins-zip-file (io/file unpack-path "builtins" "builtins.zip")
        resources (:tree (resource/load-zip-resources workspace builtins-zip-file))]
    {:resources resources
     :status-map (into {}
                       (comp resource/xform-recursive-resources
                             (map (fn [resource]
                                    (pair (resource/proj-path resource)
                                          {:version :constant
                                           :source :builtins}))))
                       resources)}))

(def make-builtins-snapshot (fn/memoize make-builtins-snapshot-raw))

(def reserved-proj-paths #{"/builtins" "/build" "/.internal" "/.git" "/.editor_settings"})

(defn reserved-proj-path? [^File root path]
  (or (reserved-proj-paths path)
      (resource/ignored-project-path? root path)))

(defn- file-resource-filter [^File root ^File f]
  (not (or (let [file-name (.getName f)]
             (= file-name ".DS_Store"))
           (reserved-proj-path? root (resource/file->proj-path root f)))))

(defn- make-file-tree
  ([workspace ^File file]
   (let [basis (g/now)
         project-directory (resource/project-directory basis workspace)
         editable-proj-path? (g/raw-property-value basis workspace :editable-proj-path?)
         unloaded-proj-path? (g/raw-property-value basis workspace :unloaded-proj-path?)]
     (make-file-tree workspace project-directory file editable-proj-path? unloaded-proj-path?)))
  ([workspace ^File root ^File file editable-proj-path? unloaded-proj-path?]
   (let [children (into []
                        (comp (filter (partial file-resource-filter root))
                              (map #(make-file-tree workspace root % editable-proj-path? unloaded-proj-path?)))
                        (.listFiles file))]
     (resource/make-file-resource workspace (.getPath root) file children editable-proj-path? unloaded-proj-path?))))

(defn- file-resource-status [resource]
  (assert (resource/file-resource? resource))
  {:version (str (.lastModified ^File (io/file resource)))
   :source :directory})

(defn file-resource-status-map-entry [resource]
  (pair (resource/proj-path resource)
        (file-resource-status resource)))

(defn file-resource-status-map-entry? [[proj-path {:keys [version source]}]]
  (and (string? proj-path)
       (str/starts-with? proj-path "/")
       (= :directory source)
       (try
         (Long/parseUnsignedLong version)
         true
         (catch NumberFormatException _
           false))))

(defn- make-directory-snapshot [workspace ^File root]
  (assert (and root (.isDirectory root)))
  (let [resources (resource/children (make-file-tree workspace root))]
    {:resources resources
     :status-map (into {}
                       (comp resource/xform-recursive-resources
                             (map file-resource-status-map-entry))
                       resources)}))

(def empty-snapshot
  {:resources []
   :status-map {}
   :errors []})

(defn map-intersection
  "Given 2 maps, return a vector of keys present in both maps"
  [m1 m2]
  (if (< (count m2) (count m1))
    (recur m2 m1)
    (persistent!
      (reduce-kv
        (fn [acc k _]
          (cond-> acc (contains? m2 k) (conj! k)))
        (transient [])
        m1))))

(defn- combine-snapshots [snapshots]
  (reduce
    (fn [result snapshot]
      (if-let [collisions (not-empty (map-intersection (:status-map result) (:status-map snapshot)))]
        (update result :errors conj {:type :collision :collisions (select-keys (:status-map snapshot) collisions)})
        (-> result
            (update :resources into (:resources snapshot))
            (update :status-map merge (:status-map snapshot)))))
    empty-snapshot
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
        resources (resource/children (make-file-tree workspace mount-root root fn/constantly-false fn/constantly-false))]
    {:resources resources
     :status-map (into {}
                       (comp resource/xform-recursive-resources
                             (map file-resource-status-map-entry))
                       resources)}))

(defn update-snapshot-status [snapshot file-resource-status-map-entries]
  (assert (every? file-resource-status-map-entry? file-resource-status-map-entries))
  (update snapshot :status-map into file-resource-status-map-entries))

(defn make-snapshot-info [workspace project-directory library-uris snapshot-cache]
  (resource/with-defignore-pred project-directory
    (let [lib-states (library/current-library-state project-directory library-uris)
          new-library-snapshot-cache (update-library-snapshot-cache snapshot-cache workspace lib-states)]
      {:snapshot (combine-snapshots (list* (make-builtins-snapshot workspace)
                                           (make-directory-snapshot workspace project-directory)
                                           (make-debugger-snapshot workspace)
                                           (make-library-snapshots new-library-snapshot-cache lib-states)))
       :snapshot-cache new-library-snapshot-cache})))

(defn make-resource-map [snapshot]
  (into {}
        (comp resource/xform-recursive-resources
              (coll/pair-map-by resource/proj-path))
        (:resources snapshot)))

(defn- resource-status [snapshot path]
  (get-in snapshot [:status-map path]))

(defn diff [old-snapshot new-snapshot]
  (let [old-map (make-resource-map old-snapshot)
        new-map (make-resource-map new-snapshot)
        old-paths (set (keys old-map))
        new-paths (set (keys new-map))
        common-paths (set/intersection new-paths old-paths)
        changed-paths (filterv #(not= (resource-status old-snapshot %)
                                      (resource-status new-snapshot %))
                               common-paths)

        {changed-from-folder-to-file :file
         changed-from-file-to-folder :folder}
        (coll/reduce->
          changed-paths
          {:file (sorted-set-by coll/descending-order)
           :folder (sorted-set-by coll/descending-order)}
          (fn [acc path]
            (let [old-source-type (resource/source-type (get old-map path))
                  new-source-type (resource/source-type (get new-map path))]
              (cond-> acc
                      (not= old-source-type new-source-type)
                      (update new-source-type conj path)))))

        added-paths (into changed-from-folder-to-file (set/difference new-paths old-paths))
        removed-paths (into changed-from-file-to-folder (set/difference old-paths new-paths))
        changed-paths (coll/into-> changed-paths (sorted-set-by coll/descending-order)
                        (remove changed-from-folder-to-file)
                        (remove changed-from-file-to-folder))
        added (mapv new-map added-paths)
        removed (mapv old-map removed-paths)
        changed (mapv new-map changed-paths)]

    (assert (empty? (set/intersection (set added) (set removed))))
    (assert (empty? (set/intersection (set added) (set changed))))
    (assert (empty? (set/intersection (set removed) (set changed))))
    {:added added
     :removed removed
     :changed changed}))

(defn empty-diff? [diff]
  (not (or (seq (:added diff))
           (seq (:removed diff))
           (seq (:changed diff)))))
