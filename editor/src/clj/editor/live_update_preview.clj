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

(ns editor.live-update-preview
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.build :as build]
            [editor.defold-project :as project]
            [editor.pipeline.bob :as bob]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace]
            [internal.java :as java]
            [util.coll :as coll]
            [util.digest :as digest])
  (:import [com.dynamo.bob.archive.liveupdate PackageArtifacts PackagePreview PackagePreview$Result PackageResolver PackageResolver$Loading PackageResolver$Resource]
           [com.dynamo.liveupdate.proto LiveUpdateManifest$ManifestDesc LiveUpdateManifest$PackageDesc]
           [java.io ByteArrayInputStream InputStream]
           [java.nio.file CopyOption DirectoryNotEmptyException Files LinkOption OpenOption Path StandardCopyOption]
           [java.nio.file.attribute FileAttribute]
           [org.apache.commons.io FileUtils IOUtils]))

(set! *warn-on-reflection* true)

(defn source-resources [resource-map generated-paths]
  (coll/into-> resource-map {}
    (filter (fn [[path source]]
              (and (= :file (resource/source-type source))
                   (not (PackageArtifacts/isGeneratedPath path generated-paths))
                   (or (resource/file-resource? source) (resource/zip-resource? source)))))))

(defn resolve-graph [project manifest-node evaluation-context cancelled?]
  (let [check-cancelled! (fn check-cancelled! []
                           (when (cancelled?) (throw (java.util.concurrent.CancellationException.))))
        _ (check-cancelled!)
        ;; As in the normal editor build, evaluate the captured graph on the
        ;; worker thread. The tracer also cancels during uncached evaluation.
        evaluation-context (assoc evaluation-context :tracer (fn [_ _ _ _] (check-cancelled!)))
        workspace (project/workspace project evaluation-context)
        resources (source-resources (g/node-value workspace :resource-map evaluation-context)
                                    (g/node-value project :live-update-output-paths evaluation-context))
        manifest (protobuf/map->pb LiveUpdateManifest$ManifestDesc (g/node-value manifest-node :save-value evaluation-context))
        directory (g/node-value manifest-node :resolved-output-directory evaluation-context)
        selected (g/node-value manifest-node :selected-paths evaluation-context)
        game-project (project/get-resource-node project "/game.project" evaluation-context)
        roots (g/node-value game-project :build-targets evaluation-context)
        custom-paths (coll/into-> (g/node-value game-project :custom-resource+versions evaluation-context) #{}
                       (map (comp resource/proj-path first)))]
    (when-let [error (g/flatten-errors [selected roots directory])]
      (throw (ex-info (@(workspace/localization workspace evaluation-context) (g/error-message error)) {:error error})))
    (let [game-target (first roots)
          ;; This view may edit a manifest other than the active game.project
          ;; setting. Replace its selections; keep startup and custom resources.
          game-target (assoc game-target
                        :dynamic-resource-deps (filterv (complement custom-paths) selected)
                        :dynamic-custom-resource-deps (vec custom-paths))
          targets (build/resolve-dependencies [game-target] project evaluation-context)]
      (when (g/error? targets)
        (throw (ex-info (@(workspace/localization workspace evaluation-context) (g/error-message targets)) {:error targets})))
      (let [targets (filterv #(not (contains? #{"project" "texture_profiles"} (resource/ext (:resource (:resource %))))) targets)
            source-path (fn source-path [target]
                          (or (resource/proj-path (:resource (:resource target)))
                              (some-> (resource-node/owner-resource (:basis evaluation-context) (:node-id target)) resource/proj-path)))
            source-outputs (reduce (fn [mapping target]
                                     (check-cancelled!)
                                     (if-let [source (source-path target)]
                                       (update mapping source (fnil conj []) (resource/proj-path (:resource target)))
                                       mapping))
                                   (coll/pair-map-by identity (constantly []) (coll/keys resources))
                                   targets)
            primary-sources (coll/into-> targets {}
                              (keep (fn [target]
                                      (when-let [source (source-path target)]
                                        [(resource/proj-path (:resource target)) source]))))
            graph (coll/into-> targets {}
                    (map (fn [target]
                           (check-cancelled!)
                           (let [output (:resource target)
                                 dynamic (get-in target [:user-data :pb-map :load-dynamically])
                                 loading (case (resource/ext output)
                                           "collectionproxyc" PackageResolver$Loading/DEFERRED
                                           "factoryc" (if dynamic PackageResolver$Loading/DEFERRED PackageResolver$Loading/EAGER)
                                           "collectionfactoryc" (if dynamic PackageResolver$Loading/DESCRIPTOR PackageResolver$Loading/EAGER)
                                           PackageResolver$Loading/EAGER)]
                             [(resource/proj-path output)
                              (PackageResolver$Resource.
                                (coll/into-> (:deps target) {}
                                  (map #(vector (resource/proj-path (:resource %)) loading))))]))))
            bootstrap (coll/into-> (get-in game-target [:user-data :path->built-resource-settings]) []
                        (remove #(= ["graphics" "texture_profiles"] (key %)))
                        (keep #(some-> (val %) resource/proj-path)))
            plan (PackageResolver/resolve manifest source-outputs primary-sources graph bootstrap custom-paths true)
            paths (coll/into-> (.getPackagesList manifest) {}
                    (map (fn [^LiveUpdateManifest$PackageDesc package]
                           [(.getName package)
                            [(str (case (.getProvider package)
                                    "zip" (.resolve ^Path directory (str (.getName package) ".zip"))
                                    "folder" (.resolve ^Path directory (.getName package))
                                    ;; Custom names need final payloads.
                                    directory))]])))]
        (check-cancelled!)
        (PackagePreview$Result. plan paths)))))

(defn controller []
  (atom {:closed false :job nil :result nil :error nil
         :close-completion (promise)
         :directory (delay (Files/createTempDirectory "defold-liveupdate-preview-" (make-array FileAttribute 0)))
         :source-versions (atom {})}))

(defn close! [controller]
  (let [[previous _] (swap-vals! controller assoc :closed true :job nil :result nil)]
    (when-not (:closed previous)
      (when-let [job (:job previous)]
        (reset! (:cancelled job) true))
      (future
        (try
          (when-let [completion (get-in previous [:job :completion])]
            @completion)
          (when (realized? (:directory previous))
            (FileUtils/deleteDirectory (.toFile ^Path @(:directory previous))))
          (finally
            (deliver (:close-completion previous) true)))))
    (:close-completion previous)))

(defn invalidate! [controller]
  (let [[previous _] (swap-vals! controller dissoc :fingerprint :error)]
    (when-let [job (:job previous)]
      (reset! (:cancelled job) true))))

(defn update! [controller fingerprint work!]
  (let [job {:cancelled (atom false) :completion (promise)}
        [previous current] (swap-vals! controller
                                       #(if (or (:closed %) (= fingerprint (:fingerprint %)))
                                          %
                                          (assoc % :fingerprint fingerprint :job job :error nil)))]
    (when (identical? job (:job current))
      (when-let [previous-job (:job previous)]
        (reset! (:cancelled previous-job) true))
      (future
        (try
          ;; Wait for superseded work to clean up before compiling again. Queued
          ;; requests that have already been replaced never invoke their work.
          (when-let [completion (get-in previous [:job :completion])]
            @completion)
          (when-not @(:cancelled job)
            (let [result (work! #(deref (:cancelled job)))]
              (swap! controller
                     (fn [state]
                       (if-not (and (identical? job (:job state))
                                    (= fingerprint (:fingerprint state))
                                    (not @(:cancelled job)))
                         state
                         (assoc state :result {:fingerprint fingerprint :value result}))))))
          (catch Exception error
            (when-not @(:cancelled job)
              (swap! controller #(if-not (and (identical? job (:job %)) (= fingerprint (:fingerprint %)))
                                   %
                                   (assoc % :error error)))))
          (finally
            (swap! controller #(if-not (identical? job (:job %)) % (assoc % :job nil)))
            (deliver (:completion job) true))))
      (:completion job))))

(defn build!
  "Prepare Bob's exact resource sets for one-off legacy conversion."
  [controller resources resource-statuses contents manifest-path options cancelled?]
  (let [^Path snapshot @(:directory @controller)
        source-versions (:source-versions @controller)
        ;; Bob mounts its own builtins, so do not copy them into the snapshot.
        resources (coll/into-> resources {} (remove #(.startsWith ^String (key %) "/builtins/")))
        current-paths (set (coll/keys resources))
        previous-versions @source-versions]
    (doseq [[path] previous-versions :when (not (contains? current-paths path))]
      (let [destination (.resolve snapshot ^String (subs path 1))]
        (Files/deleteIfExists destination)
        (loop [parent (.getParent destination)]
          (when (and (not= snapshot parent)
                     (try (Files/deleteIfExists parent) (catch DirectoryNotEmptyException _ false)))
            (recur (.getParent parent))))))
    ;; Record paths first, so the next refresh also cleans cancelled copies.
    (reset! source-versions (coll/pair-map-by identity previous-versions current-paths))
    (doseq [[path source] resources]
      (when (cancelled?) (throw (java.util.concurrent.CancellationException.)))
      (let [destination (.resolve snapshot ^String (subs path 1))
            content (get contents path)
            version [source (get resource-statuses path) (some-> content digest/string->sha1-hex)]]
        (when-not (and (= version (get previous-versions path))
                       (Files/isRegularFile destination (make-array LinkOption 0)))
          ;; A failed copy must not leave the previous version marked valid.
          (swap! source-versions assoc path nil)
          (let [input! (fn input! []
                         (if content
                           (ByteArrayInputStream. (.getBytes ^String content java.nio.charset.StandardCharsets/UTF_8))
                           (io/input-stream source)))]
            (when-not (and (Files/isRegularFile destination (make-array LinkOption 0))
                           (with-open [^InputStream input (input!)
                                       previous (Files/newInputStream destination (make-array OpenOption 0))]
                             (IOUtils/contentEquals input previous)))
              (Files/createDirectories (.getParent destination) (make-array FileAttribute 0))
              (with-open [^InputStream input (input!)]
                (Files/copy input destination ^"[Ljava.nio.file.CopyOption;" (into-array CopyOption [StandardCopyOption/REPLACE_EXISTING])))))
          (swap! source-versions assoc path version))))
    (PackagePreview/build snapshot (.toPath (.getParentFile (io/file (resource/abs-path (get resources "/game.project")))))
                          manifest-path options java/class-loader (bob/->progress (constantly nil) cancelled?))))
