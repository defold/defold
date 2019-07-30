(ns editor.build-target-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.game-project :as game-project]
            [editor.pipeline :as pipeline]
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

(defn- digest-string [value]
  (with-open [writer (StringWriter.)]
    (digestable/digest! value writer)
    (.flush writer)
    (.toString writer)))

(defn- make-lua-memory-resource [workspace source]
  (assert (string? source))
  (let [resource-type (workspace/get-resource-type workspace "lua")]
    (resource/make-memory-resource workspace resource-type source)))

(defn- make-fake-file-resource [workspace path text]
  (let [root-dir (workspace/project-path workspace)]
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
          memory-resource (make-lua-memory-resource workspace "return {key = 123}")
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

          "#dg/Bytes [0, 32, 64]" (byte-array [0 32 64])
          "#dg/ByteString [65, 66, 67]" (ByteString/copyFrom "ABC" "UTF-8")
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

      (testing "Allowed node id entries in maps"
        ;; Node ids are only allowed in maps. The key must end with "node-id",
        ;; and the node must have a :sha256 output. The other tests below will
        ;; fail in case these restrictions are violated.
        (are [result value]
          (and (= result (digest-string value))
               (= result (digest-string (into (sorted-map) value))))

          "{:node-id #dg/Node [editor.image/ImageNode, \"620a6d01e955915786374a5eaf8250bdd417a99e3839ca935e97c77faef3a431\"]}"
          {:node-id zip-resource-node}

          "{:_node-id #dg/Node [editor.image/ImageNode, \"620a6d01e955915786374a5eaf8250bdd417a99e3839ca935e97c77faef3a431\"]}"
          {:_node-id zip-resource-node}

          "{:any-key-that-ends-in-node-id #dg/Node [editor.image/ImageNode, \"620a6d01e955915786374a5eaf8250bdd417a99e3839ca935e97c77faef3a431\"]}"
          {:any-key-that-ends-in-node-id zip-resource-node}))

      (testing "ByteString UTF-8 code points"
        (is (= "#dg/ByteString [-30, -122, -110]"
               (digest-string (ByteString/copyFrom "\u2192" "UTF-8"))))
        (is (= "\u2192"
               (.toString (ByteString/copyFrom (byte-array [-30 -122 -110])) "UTF-8")))))))

(defn- built-resource-node? [resource-node]
  (not (g/node-instance? PlaceholderResourceNode resource-node)))

(defn- build-targets-or-error [resource-node]
  (when (built-resource-node? resource-node)
    (try
      (g/node-value resource-node :build-targets)
      (catch Throwable error
        (g/error-fatal (.getMessage error))))))

(defn- all-build-targets [project]
  (transduce (comp (map second)
                   (map build-targets-or-error)
                   (remove g/error?)
                   (remove empty?))
             (completing
               (fn [all-build-targets resource-build-targets]
                 (let [seen-content-hashes (into #{}
                                                 (map :content-hash)
                                                 all-build-targets)]
                   (into all-build-targets
                         (pipeline/flatten-build-targets
                           resource-build-targets seen-content-hashes)))))
             []
             (sort-by key
                      (g/node-value project :nodes-by-resource-path))))

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
                                           (pipeline/flatten-build-targets build-targets)
                                           nil
                                           (catch Throwable error
                                             [resource-path (g/error-fatal (.getMessage error))])))))
                               resource-results)]
      (is (= {} build-target-errors))
      (is (= {} flatten-errors)))))

(deftest build-target-content-hashes-are-unaffected-by-node-ids
  ;; This test ensures the build target content hashes do not change in case a
  ;; node id differs between editing sessions. If a node id is needed inside a
  ;; build target, it must be in a map under a key that ends with "node-id".
  ;; The node itself must have a :sha256 output. All resource nodes have this.
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
