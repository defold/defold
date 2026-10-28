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

(ns editor.live-update-migration
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.collection-proxy :as collection-proxy]
            [editor.defold-project :as project]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [util.coll :as coll])
  (:import [com.dynamo.bob.archive.liveupdate PackageOutputPath PackageResolver PackageResolver$Diagnostic PackageResolver$Severity]
           [com.dynamo.liveupdate.proto LiveUpdateManifest$ManifestDesc]
           [java.io File]
           [java.nio.file Files LinkOption Path]
           [java.util.function Function]
           [java.util.regex Pattern]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- proxy-collection-paths [evaluation-context]
  (let [basis (:basis evaluation-context)]
    (reduce (fn [paths node-id]
              (if-not (g/node-instance? basis collection-proxy/CollectionProxyNode node-id)
                paths
                (let [collection (g/node-value node-id :collection evaluation-context)]
                  (if-not (resource/resource? collection)
                    paths
                    (update paths
                            (if (g/node-value node-id :exclude evaluation-context) :excluded :included)
                            conj (resource/proj-path collection))))))
            {:excluded (sorted-set) :included (sorted-set)}
            (g/node-ids basis))))

(defn- migration-manifest [^File project-directory {:keys [excluded included]} settings]
  (let [provider (if (= "Folder" (get settings ["liveupdate" "mode"])) "folder" "zip")
        output-name (get settings ["liveupdate" (if (= "folder" provider) "output-folder-name" "zip-filename")] "")
        package-name (if-not (and (= "zip" provider) (string/ends-with? output-name ".zip"))
                       output-name
                       (subs output-name 0 (- (count output-name) 4)))
        output-directory (get settings ["liveupdate" (if (= "folder" provider) "output-directory" "zip-filepath")])
        output-directory (if (string/blank? output-directory) "." output-directory)
        ^Path output-root (try
                            (PackageOutputPath/resolve output-directory (.toPath project-directory)
                                                       (reify Function (apply [_ name] (System/getenv name))))
                            ;; Leave unresolved paths for manifest validation.
                            (catch IllegalArgumentException _ nil))
        compression-level (get settings ["liveupdate" "zip-compression-level"])
        rules (coll/into-> included (mapv (fn [path] {:name path :path (Pattern/quote path)}) excluded)
                (map (fn [path] {:name path :operation :remove :path (Pattern/quote path)})))
        manifest {:packages [{:name package-name :provider provider :rules rules}]}
        selection (PackageResolver/select (protobuf/map->pb LiveUpdateManifest$ManifestDesc manifest)
                                          (into (vec excluded) included))
        base-name (if (coll/any? #(= PackageResolver$Severity/ERROR (.severity ^PackageResolver$Diagnostic %))
                                (.diagnostics selection))
                    "liveupdate"
                    package-name)
        package-name (loop [suffix 0]
                       (let [candidate (str base-name (when (pos? suffix) suffix))]
                         (if (and output-root
                                  (Files/exists (.resolve output-root (str candidate (when (= "zip" provider) ".zip")))
                                                (into-array LinkOption [LinkOption/NOFOLLOW_LINKS])))
                           (recur (inc suffix))
                           candidate)))]
    (cond-> (-> manifest
               (assoc :output-directory output-directory)
               (assoc-in [:packages 0 :name] package-name))
      (and (= "zip" provider) (some? compression-level))
      (assoc :providers [{:provider "zip" :properties [{:name "compression-level" :value (str compression-level)}]}]))))

(defn- available-manifest-path [workspace resource-map]
  (loop [suffix 0]
    (let [proj-path (str "/liveupdate" (when (pos? suffix) suffix) ".lumanifest")
          manifest-resource (workspace/file-resource workspace proj-path)
          occupied (or (contains? resource-map proj-path)
                       (Files/exists (.toPath (io/file manifest-resource))
                                     (into-array LinkOption [LinkOption/NOFOLLOW_LINKS])))]
      (if occupied
        (recur (inc suffix))
        proj-path))))

(defn prepare
  "Prepare manifest content for the bundle script
  without modifying the project."
  [project evaluation-context]
  (let [workspace (project/workspace project evaluation-context)
        game-project (project/get-resource-node project "/game.project" evaluation-context)
        game-project-resource (g/node-value game-project :resource evaluation-context)
        settings (g/node-value game-project :settings-map evaluation-context)]
    (when (and (not (g/error? settings))
               (not (get settings ["liveupdate" "manifest"]))
               (not (resource/read-only? game-project-resource)))
      (let [collection-paths (proxy-collection-paths evaluation-context)
            settings-resource (get settings ["liveupdate" "settings"])
            legacy-settings (when (and settings-resource (resource/exists? settings-resource))
                              (g/node-value (project/get-resource-node project settings-resource evaluation-context)
                                            :settings-map evaluation-context))]
        (when (and (not (g/error? legacy-settings))
                   (or legacy-settings (coll/not-empty (:excluded collection-paths))))
          {:path (available-manifest-path workspace (g/node-value workspace :resource-map evaluation-context))
           :content (protobuf/map->str LiveUpdateManifest$ManifestDesc
                                      (migration-manifest (workspace/project-directory (:basis evaluation-context) workspace)
                                                          collection-paths legacy-settings))})))))
