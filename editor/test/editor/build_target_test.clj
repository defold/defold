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

(ns editor.build-target-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.build :as build]
            [editor.defold-project :as project]
            [editor.game-project :as game-project]
            [editor.placeholder-resource :refer [PlaceholderResourceNode]]
            [editor.resource :as resource]
            [editor.system :as system]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :refer [with-clean-system]]
            [util.digestable :as digestable])
  (:import [com.google.protobuf ByteString]
           [java.io StringWriter]
           [java.net URI]
           [javax.vecmath Matrix4d]))

(def ^:private project-path "test/resources/all_types_project")

(defn- digest-string
  ([value]
   (digest-string value nil))
  ([value opts]
   (with-open [writer (StringWriter.)]
     (digestable/digest! value writer opts)
     (.flush writer)
     (.toString writer))))

(defn- make-fake-file-resource [workspace path text]
  (let [root-dir (workspace/project-directory workspace)]
    (test-util/make-fake-file-resource workspace
                                       (.getPath root-dir)
                                       (io/file root-dir path)
                                       (.getBytes text "UTF-8"))))

(defn- mock-module-level-function []
  nil)

(deftest digest-strings
  ;; Here we test the strings that will be hashed during the digest process.
  ;; Note that the actual digest process writes these strings to a special
  ;; digest output stream which simply updates the hash value. This saves us
  ;; from having the entire string representation of an object in memory.
  (test-util/with-loaded-project project-path
    (let [file-resource (workspace/find-resource workspace "/test.wav")
          file-resource-node (project/get-resource-node project file-resource)
          zip-resource (workspace/find-resource workspace  "/builtins/graphics/particle_blob.png")
          zip-resource-node (project/get-resource-node project zip-resource)
          memory-resource (workspace/make-memory-resource workspace :editable "lua" "return {key = 123}")
          build-resource (workspace/make-build-resource memory-resource)
          fake-file-resource (make-fake-file-resource workspace "docs/readme.txt" "Defold")
          custom-resource (game-project/->CustomResource fake-file-resource)]
      (is (resource/exists? file-resource))
      (is (g/node-id? file-resource-node))
      (is (resource/exists? zip-resource))
      (is (g/node-id? zip-resource-node))

      (testing "Simple values"
        (are [result value]
          (= result (digest-string value))

          "nil" nil
          "true" true
          "false" false
          "1" 1
          "1.0" 1.0
          "1/2" 1/2
          "64" (byte 64)
          "\"string\"" "string"
          ":keyword" :keyword
          "symbol" 'symbol))

      (testing "Resource values"
        (are [result value]
          (= result (digest-string value))

          "#dg/BuildResource -479253108" build-resource
          "#dg/CustomResource 991186313" custom-resource
          "#dg/FakeFileResource 991186313" fake-file-resource
          "#dg/FileResource 2064822056" file-resource
          "#dg/MemoryResource -479253108" memory-resource
          "#dg/ZipResource 1001491596" zip-resource))

      (testing "Other supported values"
        (are [result value]
          (= result (digest-string value))

          "#dg/Bytes 0x002040" (byte-array [0 32 64])
          "#dg/ByteString 0x414243" (ByteString/copyFrom "ABC" "UTF-8")
          "#dg/Class java.io.StringWriter" StringWriter
          "#dg/Function editor.build-target-test/mock-module-level-function" mock-module-level-function
          "#dg/Matrix4d [1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0]" (doto (Matrix4d.) .setIdentity)
          "#dg/URI \"https://www.defold.com\"" (URI. "https://www.defold.com")))

      (testing "Collections"
        (are [result value]
          (= result (digest-string value))

          ;; Sequential
          "[]" []
          "[]" (list)
          "[nil]" [nil]
          "[0, 1, 2]" (range 3)
          "[:class, #dg/Class java.io.StringWriter]" [:class StringWriter]

          ;; Maps
          "{}" {}
          "{}" (sorted-map)
          "{:a 1, :b 2}" {:a 1 :b 2}
          "{:a 1, :b 2}" {:b 2 :a 1} ; Keys are always sorted.

          ;; Sets
          "#{}" #{}
          "#{}" (sorted-set)
          "#{nil}" #{nil}
          "#{:a, :b}" #{:a :b}
          "#{:a, :b}" #{:b :a})) ; Entries are always sorted.

      (testing "Nested collections"
        (are [result value]
          (= result (digest-string value))

          "[[#dg/Class java.lang.Byte]]"
          [[Byte]]

          "[{#dg/Class java.lang.Byte #dg/Class java.lang.Byte}]"
          [{Byte Byte}]

          "{[#dg/Class java.lang.Byte] {#dg/Class java.lang.Byte #dg/Class java.lang.Byte}}"
          {[Byte] {Byte Byte}}))

      (testing "Ignored entries in maps"
        (are [value]
          (and (= "{}" (digest-string value))
               (= "{}" (digest-string (into (sorted-map) value))))

          {:digest-ignored/node-id zip-resource-node}
          {:digest-ignored/error-node-id zip-resource-node}
          {:digest-ignored/any-key-that-ends-in-node-id zip-resource-node}))

      (testing "Allowed node id entries in maps"
        ;; Node ids are only allowed in maps. The key must end with "node-id",
        ;; and the node-id must be included in a :node-id->persistent-value map
        ;; in the opts map provided to the digest! function. The other tests
        ;; below will fail in case these restrictions are violated.
        (let [opts {:node-id->persistent-value {zip-resource-node (resource/proj-path zip-resource)}}]
          (are [result value]
            (and (= result (digest-string value opts))
                 (= result (digest-string (into (sorted-map) value) opts)))

            "{:node-id #dg/Node \"/builtins/graphics/particle_blob.png\"}"
            {:node-id zip-resource-node}

            "{:_node-id #dg/Node \"/builtins/graphics/particle_blob.png\"}"
            {:_node-id zip-resource-node}

            "{:any-key-that-ends-in-node-id #dg/Node \"/builtins/graphics/particle_blob.png\"}"
            {:any-key-that-ends-in-node-id zip-resource-node})))

      (testing "Byte array details"
        (is (= "#dg/Bytes 0x0" (digest-string (byte-array []))))
        (is (= "#dg/Bytes 0x00" (digest-string (byte-array [0]))))
        (is (= "#dg/Bytes 0x1122334455"
               (digest-string (byte-array [0x11 0x22 0x33 0x44 0x55]))))
        (is (= "#dg/Bytes 0x000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9fa0a1a2a3a4a5a6a7a8a9aaabacadaeafb0b1b2b3b4b5b6b7b8b9babbbcbdbebfc0c1c2c3c4c5c6c7c8c9cacbcccdcecfd0d1d2d3d4d5d6d7d8d9dadbdcdddedfe0e1e2e3e4e5e6e7e8e9eaebecedeeeff0f1f2f3f4f5f6f7f8f9fafbfcfdfeff"
               (digest-string (byte-array (range 0 256)))))
        (is (= "#dg/Bytes 0x808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9fa0a1a2a3a4a5a6a7a8a9aaabacadaeafb0b1b2b3b4b5b6b7b8b9babbbcbdbebfc0c1c2c3c4c5c6c7c8c9cacbcccdcecfd0d1d2d3d4d5d6d7d8d9dadbdcdddedfe0e1e2e3e4e5e6e7e8e9eaebecedeeeff0f1f2f3f4f5f6f7f8f9fafbfcfdfeff000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f"
               (digest-string (byte-array (range -128 128))))))

      (testing "ByteString details"
        (is (= "#dg/ByteString 0x0"
               (digest-string (ByteString/copyFrom "" "UTF-8"))))
        (is (= "#dg/ByteString 0x00"
               (digest-string (ByteString/copyFrom "\0" "UTF-8"))))
        (is (= "#dg/ByteString 0x1122334455"
               (digest-string (ByteString/copyFrom (byte-array [0x11 0x22 0x33 0x44 0x55])))))
        (is (= "#dg/ByteString 0xe28692"
               (digest-string (ByteString/copyFrom "\u2192" "UTF-8")))))))) ; Rightwards arrow.

(defn- built-resource-node? [resource-node]
  (not (g/node-instance? PlaceholderResourceNode resource-node)))

(defn- build-targets-or-error [resource-node]
  (when (built-resource-node? resource-node)
    (try
      (g/node-value resource-node :build-targets)
      (catch Throwable error
        (g/error-fatal (.getMessage error)
                       (ex-data error))))))

(defn- all-build-targets [project]
  (build/resolve-dependencies
    (->> (g/node-value project :nodes-by-resource-path)
         (sort-by key)
         (map val)
         (map build-targets-or-error)
         (remove g/error?)
         (remove empty?))
    project))

(defn- build-target-content-hashes-by-path [project]
  (into (sorted-map)
        (map (juxt (comp resource/proj-path :resource)
                   :content-hash))
        (all-build-targets project)))

(deftest build-targets-produce-no-errors
  (test-util/with-loaded-project project-path
    (let [resource-nodes-by-path (g/node-value project :nodes-by-resource-path)
          resource-results (keep (fn [[proj-path resource-node]]
                                   (let [build-targets (build-targets-or-error resource-node)]
                                     (when (seq build-targets)
                                       [proj-path build-targets])))
                                 (sort-by key resource-nodes-by-path))
          build-target-errors (into (sorted-map)
                                    (filter (comp g/error? second))
                                    resource-results)
          flatten-errors (into (sorted-map)
                               (keep (fn [[resource-path build-targets]]
                                       (when-not (g/error? build-targets)
                                         (try
                                           (build/resolve-dependencies build-targets project)
                                           nil
                                           (catch Throwable error
                                             [resource-path (g/error-fatal (.getMessage error))])))))
                               resource-results)]
      (is (= {} build-target-errors))
      (is (= {} flatten-errors)))))

(deftest build-target-content-hashes-are-unaffected-by-node-ids
  ;; This test ensures the build target content hashes do not change in case a
  ;; node id differs between editing sessions. If a node id is needed inside a
  ;; build target, it must be in a map under a key that ends with "node-id", and
  ;; a :node-id->persistent-value map must be provided in the opts map argument
  ;; to build-target/with-content-hash.
  (let [session1-content-hashes-by-path
        (test-util/with-loaded-project project-path
          (build-target-content-hashes-by-path project))

        session2-content-hashes-by-path
        (with-clean-system
          (g/make-graph!) ; This causes all node ids to differ from session1.
          (let [workspace (test-util/setup-workspace! world project-path)
                project (test-util/setup-project! workspace)]
            (build-target-content-hashes-by-path project)))]

    (doseq [[path session1-content-hash] session1-content-hashes-by-path
            :let [session2-content-hash (session2-content-hashes-by-path path)]]
      (is (= session1-content-hash session2-content-hash)))))

(deftest build-target-content-hashes-are-affected-by-engine-revision
  ;; We cannot safely reuse build artifacts that were compiled for a different
  ;; version of the engine runtime, since external tools or libraries might have
  ;; been changed. The converse also hold true. We can safely reuse build
  ;; artifacts that were produced by the editor to target a particular engine,
  ;; even if the build function or any of the functions it calls have changed.
  ;; This means the build cache will be invalidated by engine upgrades, but can
  ;; be reused as long as only the editor code has changed.
  (let [defold-engine-sha1 (system/defold-engine-sha1)]
    (try
      (let [rev1-content-hashes-by-path
            (with-clean-system
              (system/set-defold-engine-sha1! "1111111111111111111111111111111111111111")
              (let [workspace (test-util/setup-workspace! world project-path)
                    project (test-util/setup-project! workspace)]
                (build-target-content-hashes-by-path project)))

            rev2-content-hashes-by-path
            (with-clean-system
              (system/set-defold-engine-sha1! "2222222222222222222222222222222222222222")
              (let [workspace (test-util/setup-workspace! world project-path)
                    project (test-util/setup-project! workspace)]
                (build-target-content-hashes-by-path project)))]

        (doseq [[path rev1-content-hash] rev1-content-hashes-by-path
                :let [rev2-content-hash (rev2-content-hashes-by-path path)]]
          (is (not= rev1-content-hash rev2-content-hash))))
      (finally
        (when (not-empty defold-engine-sha1)
          (system/set-defold-engine-sha1! defold-engine-sha1))))))
