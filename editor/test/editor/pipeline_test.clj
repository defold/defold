(ns editor.pipeline-test
  (:require
   [clojure.test :refer :all]
   [clojure.java.io :as io]
   [integration.test-util :as test-util]
   [editor.fs :as fs]
   [editor.pipeline :as pipeline]
   [editor.protobuf :as protobuf]
   [editor.progress :as progress]
   [editor.resource :as resource]
   [editor.workspace :as workspace]
   [support.test-support :as ts])
  (:import
   [com.dynamo.sprite.proto Sprite$SpriteDesc Sprite$SpriteDesc$BlendMode]
   [java.io ByteArrayOutputStream File]
   [org.apache.commons.io IOUtils]))

(def project-path "test/resources/custom_resources_project")

(defn- make-asserting-build-target
  [workspace id callback dep-resources & deps]
  (let [build-resource (workspace/make-build-resource (workspace/file-resource workspace (str "/resource-" id)))
        user-data (str "user-data-" id)]
    {:resource  build-resource
     :build-fn  (fn [resource' dep-resources' user-data']
                  (when callback (callback))
                  (is (= build-resource resource'))
                  (is (= dep-resources dep-resources'))
                  (is (= user-data user-data'))
                  {:resource resource'
                   :content (.getBytes id)})
     :user-data user-data
     :deps      (vec deps)}))

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
          (workspace/reset-cache! workspace)
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
      (testing "invokes build-fn when the target key has changed (build-fn recreated)"
        (let [build-targets [(-> (make-asserting-build-target workspace "1" called! {})
                               (assoc :build-fn (fn [resource' dep-resources' user-data']
                                                  (called!)
                                                  {:resource resource' :content (.getBytes "1")})))]
              _ (pipeline-build! workspace build-targets)
              build-results (pipeline-build! workspace build-targets)]
          (is (= 5 @build-fn-calls))
          (is (= "1" (content (first (:artifacts build-results)))))
          (let [build-results (pipeline-build! workspace
                                               (assoc-in build-targets [0 :user-data] {:new-value 42}))]
            (is (= 6 @build-fn-calls))
            (is (= "1" (content (first (:artifacts build-results))))))))
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

(deftest make-pb-map-build-target-test
  (with-clean-system
    (let [tile-set-target (make-asserting-build-target workspace "1" nil {})
          material-target (make-asserting-build-target workspace "2" nil {})
          source-resource (workspace/file-resource workspace "/dir/test.sprite")
          sprite-target   (pipeline/make-pb-map-build-target
                            12345
                            source-resource
                            [tile-set-target material-target]
                            Sprite$SpriteDesc
                            {:tile-set          (-> tile-set-target :resource :resource)
                             :default-animation "gurka"
                             :material          (-> material-target :resource :resource)})]
      (testing "produces correct build-target"
        (is (= 12345 (:node-id sprite-target)))
        (is (= source-resource (-> sprite-target :resource :resource)))
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
