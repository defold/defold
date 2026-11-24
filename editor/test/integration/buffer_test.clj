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

(ns integration.buffer-test
  (:require [clojure.data.json :as json]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.build :as build]
            [editor.defold-project-search :as project-search]
            [editor.fs :as fs]
            [editor.localization :as localization]
            [editor.progress :as progress]
            [editor.resource :as resource]
            [editor.resource-io :as resource-io]
            [editor.resource-loading :as resource-loading]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [util.fn :as fn]
            [util.murmur :as murmur])
  (:import [java.nio ByteBuffer ByteOrder]))

(set! *warn-on-reflection* true)

(defn- color-weight-buffer-json [weight]
  (json/write-str
    [{:name "color" :type "uint8" :count 3 :data [1 2 3]}
     {:name "weight" :type "float32" :count 1 :data [weight]}]
    :indent true))

(defn- expected-buffer
  ^bytes [value]
  (let [bytes (doto (ByteBuffer/allocate 56)
                (.order ByteOrder/LITTLE_ENDIAN)
                (.put (byte-array [66 85 70 67 1 0 2 0 1 0 0 0 8 0 0 0]))
                (.putLong (murmur/hash64 "color"))
                (.putInt 0)
                (.put (byte 0))
                (.put (byte 3))
                (.putShort (short 0))
                (.putLong (murmur/hash64 "weight"))
                (.putInt 4)
                (.put (byte 8))
                (.put (byte 1))
                (.putShort (short 0))
                (.put (byte-array [1 2 3 0]))
                (.putFloat (float value)))]
    (.array bytes)))

;; An unopened buffer supplies mesh and build data directly from disk. Editing
;; it must invalidate those streams and use the unsaved text. Requesting its
;; editor lines materializes it.
(deftest buffer-shell-streams
  (test-util/with-temp-project-content
    {"/packed.buffer" "[{\"name\":\"weight\",\"type\":\"float32\",\"count\":1,\"data\":[-1.5]}]"}
    (let [node-id (test-util/resource-node project "/packed.buffer")

          expected
          [{:name "weight" :type :value-type-float32 :count 1 :data [-1.5]}]]
      (g/with-auto-evaluation-context evaluation-context
        (let [basis @(:basis-ref evaluation-context)]
          (is (g/shell-node? (g/node-by-id basis node-id)))
          (is (= [] (g/required-shell-node-ids basis [(g/endpoint node-id :build-targets)])))
          (is (= #{"weight"} (g/node-value node-id :stream-ids evaluation-context)))
          (is (= expected (get-in (g/node-value node-id :build-targets evaluation-context) [0 :user-data :streams])))
          (is (identical? basis @(:basis-ref evaluation-context)))
          (is (nil? (g/raw-property-value basis node-id :source-value)))))
      (is (g/shell-node? (g/node-by-id node-id)))
      (resource-loading/load-resources! project ["/packed.buffer"] [(g/endpoint node-id :lines)]
                                        progress/null-render-progress! fn/constantly-false
                                        :resource-output-label :build-targets)
      (is (not (g/shell-node? (g/node-by-id node-id))))
      ;; Exercise the materialized producer even if its shell result is cached.
      (g/invalidate-outputs! [(g/endpoint node-id :streams)])
      (is (= expected (g/node-value node-id :streams)))
      (test-util/set-code-editor-source! node-id "[]")
      (is (= [] (g/node-value node-id :streams)))
      (is (= #{} (g/node-value node-id :stream-ids))))))

;; A searched buffer keeps using the retained vertex data across materialization
;; until workspace sync picks up the external edit.
(deftest searched-buffer-streams-before-sync
  (let [proj-path "/searched.buffer"]
    (test-util/with-temp-project-content {proj-path (color-weight-buffer-json -1.5)}
      (let [node-id (test-util/resource-node project proj-path)
            buffer-resource (g/node-value node-id :resource)

            search-fn (:search-fn (resource/resource-type buffer-resource))

            search-value
            (g/with-auto-evaluation-context evaluation-context
              (project-search/search-value node-id buffer-resource evaluation-context))

            matches (search-fn search-value (search-fn "weight"))]
        (is (= 1 (count matches)))
        (test-util/write-file-resource! workspace proj-path (color-weight-buffer-json 7))
        (is (= [-1.5] (get-in (g/node-value node-id :streams) [1 :data])))
        (is (g/shell-node? (g/node-by-id node-id)))
        (g/with-auto-evaluation-context evaluation-context
          (g/materialize-node! node-id evaluation-context))
        (is (= [-1.5] (get-in (g/node-value node-id :streams) [1 :data])))
        (workspace/resource-sync! workspace)
        (let [synced-node-id (test-util/resource-node project proj-path)]
          (is (= [7] (get-in (g/node-value synced-node-id :streams) [1 :data]))))))))

;; Preparing and building a mesh must pack its unopened vertex buffer from the
;; shell without collecting the entire file into editor lines.
(deftest build-mesh-with-unopened-vertex-buffer
  (test-util/with-temp-project-content
    {"/colored.mesh" {:material "/builtins/materials/model.material"
                      :vertices "/vertices.buffer"}
     "/vertices.buffer" (color-weight-buffer-json -1.5)}
    (let [mesh-node-id (test-util/resource-node project "/colored.mesh")
          buffer-node-id (test-util/resource-node project "/vertices.buffer")
          buffer-output-labels (atom #{})

          trace-buffer-output
          (fn trace-buffer-output [state node-id output-type label]
            (when (and (= :begin state)
                       (= buffer-node-id node-id)
                       (= :output output-type))
              (swap! buffer-output-labels conj label)))]
      (is (g/shell-node? (g/node-by-id buffer-node-id)))
      (build/load-build-resources! project ["/colored.mesh"] progress/null-render-progress! fn/constantly-false)
      (is (g/shell-node? (g/node-by-id buffer-node-id)))
      (g/with-auto-evaluation-context evaluation-context
        (g/node-value
          mesh-node-id
          :build-targets
          (g/dry-run-evaluation-context evaluation-context trace-buffer-output)))
      (is (contains? @buffer-output-labels :streams))
      (is (not (contains? @buffer-output-labels :lines)))
      (test-util/build-node! mesh-node-id nil)
      (is (g/shell-node? (g/node-by-id buffer-node-id)))
      (is (nil? (g/node-value buffer-node-id :source-value)))
      (is (= (vec (expected-buffer -1.5))
             (vec (test-util/node-build-output buffer-node-id)))))))

;; Preparing a mesh scene must read its vertex streams without retaining the
;; unopened buffer as editor text.
(deftest mesh-scene-with-unopened-vertex-buffer
  (test-util/with-temp-project-content
    {"/colored.mesh" {:material "/builtins/materials/model.material"
                      :vertices "/vertices.buffer"}
     "/vertices.buffer" (color-weight-buffer-json -1.5)}
    (let [mesh-node-id (test-util/resource-node project "/colored.mesh")
          buffer-node-id (test-util/resource-node project "/vertices.buffer")]
      (resource-loading/load-and-evaluate-resource!
        project mesh-node-id "/colored.mesh" :scene progress/null-render-progress! fn/constantly-false)
      (let [user-data
            (get-in (g/node-value mesh-node-id :scene) [:renderable :user-data])]
        (is (= 1 (:vertex-count user-data)))
        (is (= [[1 2 3] [-1.5]] (mapv (comp vec :data) (:array-streams user-data)))))
      (is (g/shell-node? (g/node-by-id buffer-node-id)))
      (is (nil? (g/node-value buffer-node-id :source-value))))))

;; Truncated JSON and missing files produce distinct, equivalent errors before
;; and after materialization.
(deftest buffer-shell-errors
  (test-util/with-temp-project-content {"/broken.buffer" "[{" "/missing.buffer" "[]"}
    (fs/delete-file! (test-util/file workspace "/missing.buffer"))
    (doseq [path ["/broken.buffer" "/missing.buffer"]]
      (let [node-id (test-util/resource-node project path)
            shell-error (g/node-value node-id :streams)]
        (is (g/error-fatal? shell-error))
        (if (= "/broken.buffer" path)
          (is (= (localization/message "error.buffer-syntax-error") (:message shell-error)))
          (is (resource-io/file-not-found-error? shell-error)))
        (is (g/shell-node? (g/node-by-id node-id)))
        (g/with-auto-evaluation-context evaluation-context
          (g/materialize-node! node-id evaluation-context))
        (g/invalidate-outputs! [(g/endpoint node-id :streams)])
        (is (= shell-error (g/node-value node-id :streams)))))))

;; Builds pack in-memory stream data and invalidate output after unsaved edits.
(deftest build-unsaved-buffer
  (test-util/with-scratch-project "test/resources/empty_project"
    (let [node-id
          (test-util/make-code-resource-node!
            project
            "/packed.buffer"
            (color-weight-buffer-json -1.5))]
      (is (g/node-value node-id :dirty))
      (test-util/build-node! node-id nil)
      (is (= (vec (expected-buffer -1.5))
             (vec (test-util/node-build-output node-id))))
      (test-util/set-code-editor-source! node-id (color-weight-buffer-json 7))
      (test-util/build-node! node-id nil)
      (is (= (vec (expected-buffer 7))
             (vec (test-util/node-build-output node-id)))))))

;; Invalid layouts surface as build errors instead of unreadable resources.
(deftest invalid-buffer-layout
  (test-util/with-scratch-project "test/resources/empty_project"
    (let [node-id (test-util/make-code-resource-node!
                    project "/invalid.buffer"
                    "[{\"name\":\"position\",\"type\":\"float32\",\"count\":0}]")]
      (is (g/error? (test-util/build-error! node-id))))))
