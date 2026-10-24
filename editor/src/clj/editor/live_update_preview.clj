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
            [editor.pipeline.bob :as bob]
            [editor.resource :as resource]
            [internal.java :as java]
            [util.coll :as coll])
  (:import [com.dynamo.bob.archive.liveupdate PackagePreview]
           [java.io ByteArrayInputStream InputStream]
           [java.nio.file CopyOption DirectoryNotEmptyException Files LinkOption OpenOption Path StandardCopyOption]
           [java.nio.file.attribute FileAttribute]
           [org.apache.commons.io FileUtils IOUtils]))

(set! *warn-on-reflection* true)

(defn controller []
  (atom {:closed false :job nil :result nil :error nil
         :close-completion (promise)
         :directory (delay (Files/createTempDirectory "defold-liveupdate-preview-" (make-array FileAttribute 0)))
         :paths (atom #{})}))

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

(defn build! [controller resources contents manifest-path options cancelled?]
  (let [^Path snapshot @(:directory @controller)
        paths (:paths @controller)
        ;; Bob mounts its own builtins, so do not copy them into the snapshot.
        resources (coll/into-> resources {} (remove #(.startsWith ^String (key %) "/builtins/")))
        current-paths (set (coll/keys resources))
        previous-paths @paths]
    (doseq [path previous-paths :when (not (contains? current-paths path))]
      (let [destination (.resolve snapshot ^String (subs path 1))]
        (Files/deleteIfExists destination)
        (loop [parent (.getParent destination)]
          (when (and (not= snapshot parent)
                     (try (Files/deleteIfExists parent) (catch DirectoryNotEmptyException _ false)))
            (recur (.getParent parent))))))
    ;; Record paths first, so the next refresh also cleans cancelled copies.
    (reset! paths current-paths)
    (doseq [[path source] resources]
      (when (cancelled?) (throw (java.util.concurrent.CancellationException.)))
      (let [destination (.resolve snapshot ^String (subs path 1))
            input! #(if-let [content (get contents path)]
                      (ByteArrayInputStream. (.getBytes ^String content java.nio.charset.StandardCharsets/UTF_8))
                      (io/input-stream source))]
        (when-not (and (Files/isRegularFile destination (make-array LinkOption 0))
                      (with-open [^InputStream input (input!)
                                  previous (Files/newInputStream destination (make-array OpenOption 0))]
                        (IOUtils/contentEquals input previous)))
          (Files/createDirectories (.getParent destination) (make-array FileAttribute 0))
          (with-open [^InputStream input (input!)]
            (Files/copy input destination ^"[Ljava.nio.file.CopyOption;" (into-array CopyOption [StandardCopyOption/REPLACE_EXISTING]))))))
    (PackagePreview/build snapshot (.toPath (.getParentFile (io/file (resource/abs-path (get resources "/game.project")))))
                          manifest-path options java/class-loader (bob/->progress (constantly nil) cancelled?))))
