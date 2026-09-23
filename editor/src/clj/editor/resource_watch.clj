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
            [editor.system :as system]
            [util.coll :as coll :refer [pair]]
            [util.fn :as fn]
            [util.path :as path])
  (:import [com.dynamo.bob.util Library$Archive Library$Result]
           [java.io File]))

(set! *warn-on-reflection* true)

(defn- resource-root-dir [resource]
  (when-let [path-splits (seq (rest (str/split (resource/proj-path resource) #"/")))] ; skip initial ""
    (if (= (count path-splits) 1)
      (when (= (resource/source-type resource) :folder)
        (first path-splits))
      (first path-splits))))

(defn- make-library-snapshot [workspace ^Library$Result lib-result mtime]
  (let [archive (.archive lib-result)
        base-dir (.baseDir archive)
        zip-resources (resource/load-zip-resources
                        workspace
                        (.toFile (.path archive))
                        (when-not (str/blank? base-dir) base-dir))
        include-dirs (set (.includeDirs archive))
        {:keys [tree versions]} (update zip-resources :tree coll/filterv-> #(include-dirs (resource-root-dir %)))]
    {:mtime mtime
     :resources tree
     :status-map (coll/into-> tree {}
                   resource/xform-recursive-resources
                   (map (fn [resource]
                          (let [proj-path (resource/proj-path resource)]
                            (pair proj-path
                                  {:version (versions proj-path)
                                   :source :library
                                   :library (.uri lib-result)})))))}))

(defn- update-library-snapshot-cache
  [library-snapshot-cache workspace lib-results]
  (into library-snapshot-cache
        (keep (fn [^Library$Result lib-result]
                (when-some [archive (.archive lib-result)]
                  (let [archive-path (.path archive)
                        mtime (path/last-modified-ms archive-path)
                        cached-snapshot (get library-snapshot-cache archive-path)]
                    (when (or (nil? cached-snapshot)
                              (not= mtime (:mtime cached-snapshot)))
                      (pair archive-path (make-library-snapshot workspace lib-result mtime)))))))
        lib-results))

(defn- make-library-snapshots [library-snapshot-cache lib-results]
  (into []
        (comp (keep Library$Result/.archive)
              (map Library$Archive/.path)
              (map library-snapshot-cache))
        lib-results))

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

(defn- file-resource-status [resource]
  (assert (resource/file-resource? resource))
  {:version (str (.lastModified ^File (io/file resource)))
   :source :directory})

(defn file-resource-status-map-entry [resource]
  (pair (resource/proj-path resource)
        (file-resource-status resource)))

(defn file-resource-status-map-entry? [[proj-path {:keys [version source]}]]
  (and (resource/proj-path? proj-path)
       (= :directory source)
       (try
         (Long/parseUnsignedLong version)
         true
         (catch NumberFormatException _
           false))))

(defn- make-directory-snapshot [workspace ^File mount-root ^File root editable-proj-path? unloaded-proj-path?]
  (let [{:keys [tree versions]} (resource/load-directory-resources workspace mount-root root
                                                                (partial file-resource-filter mount-root)
                                                                editable-proj-path? unloaded-proj-path?)]
    {:resources tree
     :status-map (coll/into-> versions {}
                   (map (fn [[proj-path version]]
                          (pair proj-path {:version version :source :directory}))))}))

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

(defn- make-debugger-snapshot [workspace]
  (let [base-path (if (system/defold-dev?)
                    ;; Use local debugger support files so we can see
                    ;; changes to them instantly without re-packing/restarting.
                    (.getAbsolutePath (io/file "bundle-resources"))
                    (system/defold-unpack-path))
        root (io/file base-path "_defold/debugger")
        ;; This mount-root is outside the project, so file-resource-filter in
        ;; make-directory-snapshot won't apply the project's defignore patterns.
        ;; The debugger directory is not expected to contain files that need
        ;; those exclusions. It also uses neither the project's editability
        ;; nor its unloaded-resource settings.
        mount-root (io/file base-path)]
    (make-directory-snapshot workspace mount-root root fn/constantly-false fn/constantly-false)))

(defn update-snapshot-status [snapshot file-resource-status-map-entries]
  (assert (every? file-resource-status-map-entry? file-resource-status-map-entries))
  (update snapshot :status-map into file-resource-status-map-entries))

(defn make-snapshot-info [workspace project-directory library-uris snapshot-cache]
  (resource/with-defignore-pred project-directory
    (let [basis (g/now)
          editable-proj-path? (g/raw-property-value basis workspace :editable-proj-path?)
          unloaded-proj-path? (g/raw-property-value basis workspace :unloaded-proj-path?)
          lib-results (library/cached project-directory library-uris)
          new-library-snapshot-cache (update-library-snapshot-cache snapshot-cache workspace lib-results)
          snapshot (combine-snapshots (list* (make-builtins-snapshot workspace)
                                            (make-directory-snapshot workspace project-directory project-directory editable-proj-path? unloaded-proj-path?)
                                            (make-debugger-snapshot workspace)
                                            (make-library-snapshots new-library-snapshot-cache lib-results)))]
      {:snapshot snapshot
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
