(ns editor.pipeline-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [editor.build-target :as bt]
            [editor.pipeline :as pipeline]
            [editor.progress :as progress]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :as ts])
  (:import [com.dynamo.sprite.proto Sprite$SpriteDesc]
           [java.io ByteArrayOutputStream]
           [org.apache.commons.io IOUtils]))

(def project-path "test/resources/custom_resources_project")

(defn- mock-build-fn [resource _dep-resources user-data]
  {:resource resource
   :content (.getBytes (str user-data))})

(defn- make-mock-build-target
  [workspace id dep-resources & deps]
  (let [build-resource (workspace/make-build-resource (workspace/file-resource workspace (str "/resource-" id)))
        user-data (str id)]
    (bt/with-content-hash
      {:resource build-resource
       :build-fn mock-build-fn
       :user-data user-data
       :dep-resources dep-resources
       :deps (vec deps)})))

(defn- install-asserting-build-fn
  [build-target callback]
  (let [build-resource (:resource build-target)
        dep-resources (:dep-resources build-target)
        user-data (:user-data build-target)]
    (assoc build-target
      :callback callback
      :build-fn (fn [resource' dep-resources' user-data']
                  (when callback (callback))
                  (is (= build-resource resource'))
                  (is (= dep-resources dep-resources'))
                  (is (= user-data user-data'))
                  (mock-build-fn resource' dep-resources' user-data')))))

(defn- make-asserting-build-target
  [workspace id callback dep-resources & deps]
  (-> (apply make-mock-build-target workspace id dep-resources deps)
      (install-asserting-build-fn callback)))

(defn- rehash-asserting-build-target
  [asserting-build-target]
  (let [callback (:callback asserting-build-target)]

    (-> asserting-build-target
        (assoc :build-fn mock-build-fn)
        bt/with-content-hash
        (install-asserting-build-fn callback))))

(defmacro with-clean-system [& forms]
  `(ts/with-clean-system
     (let [~'workspace (test-util/setup-scratch-workspace! ~'world project-path)]
       ~@forms)))

(defn- content-bytes [artifact]
  (with-open [in (io/input-stream (:resource artifact))
              out (ByteArrayOutputStream.)]
    (IOUtils/copy in out)
    (.toByteArray out)))

(defn- content [artifact]
  (-> artifact
    (content-bytes)
    (String. "UTF-8")))

(defn- pipeline-build! [workspace build-targets]
  (let [old-artifact-map (workspace/artifact-map workspace)
        build-results (pipeline/build! build-targets (workspace/build-path workspace) old-artifact-map progress/null-render-progress!)]
    (when-not (contains? build-results :error)
      (workspace/artifact-map! workspace (:artifact-map build-results))
      (workspace/etags! workspace (:etags build-results)))
    build-results))

(deftest build-single-test
  (with-clean-system
    (let [build-fn-calls (atom 0)
          called! #(swap! build-fn-calls inc)
          build-targets [(make-asserting-build-target workspace "1" called! {})]
          build-results (pipeline-build! workspace build-targets)]
      (testing "invokes build-fn correctly for a single build-target"
        (is (= 1 @build-fn-calls))
        (let [artifact (first (:artifacts build-results))]
          (is (= "1" (content artifact)))
          (is (some? (workspace/etag workspace (resource/proj-path (:resource artifact)))))))
      (let [build-results (pipeline-build! workspace build-targets)]
        (testing "does not invoke build-fn for equivalent target"
          (is (= 1 @build-fn-calls))
          (is (= "1" (content (first (:artifacts build-results))))))
        (testing "invokes build-fn when cache is explicitly cleared"
          (workspace/clear-build-cache! workspace)
          (let [build-results (pipeline-build! workspace build-targets)]
            (is (= 2 @build-fn-calls))
            (is (= "1" (content (first (:artifacts build-results)))))))
        (let [f (io/as-file (:resource (first (:artifacts build-results))))]
          (testing "invokes build-fn when target resource is modified"
            (.setLastModified f 0)
            (let [build-results (pipeline-build! workspace build-targets)]
              (is (= 3 @build-fn-calls))
              (is (= "1" (content (first (:artifacts build-results)))))))
          (testing "invokes build-fn when target resource is deleted"
            (.delete f)
            (let [build-results (pipeline-build! workspace build-targets)]
              (is (= 4 @build-fn-calls))
              (is (= "1" (content (first (:artifacts build-results)))))))))
      (testing "invokes build-fn when the content hash has changed"
        (let [build-targets [(-> (make-asserting-build-target workspace "1" called! {})
                                 (assoc :user-data "X")
                                 rehash-asserting-build-target)]
              _ (pipeline-build! workspace build-targets)
              build-results (pipeline-build! workspace build-targets)]
          (is (= 5 @build-fn-calls))
          (is (= "X" (content (first (:artifacts build-results)))))
          (let [build-targets' (update build-targets 0 (fn [build-target]
                                                         (-> build-target
                                                             (assoc :user-data {:new-value 42})
                                                             rehash-asserting-build-target)))
                build-results (pipeline-build! workspace build-targets')]
            (is (= 6 @build-fn-calls))
            (is (= "{:new-value 42}" (content (first (:artifacts build-results))))))))
      (testing "fs is pruned"
        (let [files-before (doall (file-seq (workspace/build-path workspace)))
              build-results (pipeline-build! workspace [])
              files-after (doall (file-seq (workspace/build-path workspace)))]
          (is (> (count files-before) (count files-after))))))))

(deftest build-multi-test
  (testing "invokes build-fns correctly for multiple inter-dependant build-targets"
    (with-clean-system
      (let [build-fn-calls (atom 0)
            called!        #(swap! build-fn-calls inc)
            dep-1          (make-asserting-build-target workspace "1" called! {})
            dep-2          (make-asserting-build-target workspace "2" called! {})
            dep-3          (make-asserting-build-target workspace "3" called!
                             {(:resource dep-2) (:resource dep-2)} dep-2)
            build-targets  [(make-asserting-build-target workspace "4" called!
                              {(:resource dep-1) (:resource dep-1)
                               (:resource dep-3) (:resource dep-3)}
                              dep-1 dep-3)]
            build-results  (pipeline-build! workspace build-targets)]
        (is (= 4 @build-fn-calls))
        (is (= #{"1" "2" "3" "4"} (set (map content (:artifacts build-results)))))))))

(deftest make-protobuf-build-target-test
  (with-clean-system
    (let [tile-set-target (make-asserting-build-target workspace "1" nil {})
          material-target (make-asserting-build-target workspace "2" nil {})
          sprite-target   (pipeline/make-protobuf-build-target
                            (workspace/file-resource workspace "/dir/test.sprite")
                            [tile-set-target material-target]
                            Sprite$SpriteDesc
                            {:tile-set          (-> tile-set-target :resource :resource)
                             :default-animation "gurka"
                             :material          (-> material-target :resource :resource)}
                            [:tile-set :material])]
      (testing "produces correct build-target"
        (is (= (set (:deps sprite-target)) #{tile-set-target material-target})))
      (testing "produces correct build content"
        (let [build-results (pipeline-build! workspace [sprite-target])
              sprite-result (first (filter #(= (:resource %) (:resource sprite-target)) (:artifacts build-results)))
              pb-data (protobuf/bytes->map Sprite$SpriteDesc (content-bytes sprite-result))]
          ;; assert resource paths have been resolved to build paths
          (is (= {:tile-set (-> tile-set-target :resource resource/proj-path)
                  :default-animation "gurka"
                  :material (-> material-target :resource resource/proj-path)}
                (select-keys pb-data [:tile-set :default-animation :material]))))))))
