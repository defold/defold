(ns editor.pipeline-test
  (:require
   [clojure.test :refer :all]
   [clojure.java.io :as io]
   [integration.test-util :as test-util]
   [editor.fs :as fs]
   [editor.pipeline :as pipeline]
   [editor.protobuf :as protobuf]
   [editor.resource :as resource]
   [editor.workspace :as workspace]
   [support.test-support :as ts])
  (:import
   [com.dynamo.sprite.proto Sprite$SpriteDesc Sprite$SpriteDesc$BlendMode]
   [java.io ByteArrayOutputStream File]
   [org.apache.commons.io IOUtils]))

(def project-path "test/resources/custom_resources_project")

(defn- make-asserting-build-target
  [workspace basis id callback dep-resources & deps]
  (let [build-resource (workspace/make-build-resource (workspace/file-resource workspace (str "/resource-" id)))
        user-data (str "user-data-" id)]
    {:node-id   id
     :resource  build-resource
     :build-fn  (fn [node-id' basis' resource' dep-resources' user-data']
                  (when callback (callback))
                  (is (= id node-id'))
                  (is (= basis basis'))
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

(deftest build-single-test
  (with-clean-system
    (let [build-fn-calls (atom 0)
          called! #(swap! build-fn-calls inc)
          build-targets [(make-asserting-build-target workspace ::basis "1" called! {})]
          build-results (pipeline/build! workspace ::basis build-targets)]
      (testing "invokes build-fn correctly for a single build-target"
        (is (= 1 @build-fn-calls))
        (let [artifact (first build-results)]
          (is (= "1" (content artifact)))
          (is (some? (pipeline/etag workspace (resource/proj-path (:resource artifact)))))))
      (let [build-results (pipeline/build! workspace ::basis build-targets)]
        (testing "does not invoke build-fn for equivalent target"
          (is (= 1 @build-fn-calls))
          (is (= "1" (content (first build-results)))))
        (testing "invokes build-fn when cache is explicitly cleared"
          (pipeline/reset-cache! workspace)
          (let [build-results (pipeline/build! workspace ::basis build-targets)]
            (is (= 2 @build-fn-calls))
            (is (= "1" (content (first build-results))))))
        (let [f (io/as-file (:resource (first build-results)))]
          (testing "invokes build-fn when target resource is modified"
            (.setLastModified f 0)
            (let [build-results (pipeline/build! workspace ::basis build-targets)]
              (is (= 3 @build-fn-calls))
              (is (= "1" (content (first build-results))))))
          (testing "invokes build-fn when target resource is deleted"
            (.delete f)
            (let [build-results (pipeline/build! workspace ::basis build-targets)]
              (is (= 4 @build-fn-calls))
              (is (= "1" (content (first build-results))))))))
      (testing "invokes build-fn when the target key has changed (build-fn recreated)"
        (let [build-targets [(-> (make-asserting-build-target workspace ::basis "1" called! {})
                               (assoc :build-fn (fn [node-id' basis' resource' dep-resources' user-data']
                                                  (called!)
                                                  {:resource resource' :content (.getBytes "1")})))]
              _ (pipeline/build! workspace ::basis build-targets)
              build-results (pipeline/build! workspace ::basis build-targets)]
          (is (= 5 @build-fn-calls))
          (is (= "1" (content (first build-results))))
          (let [build-results (->> (assoc-in build-targets [0 :user-data] {:new-value 42})
                                (pipeline/build! workspace ::basis))]
            (is (= 6 @build-fn-calls))
            (is (= "1" (content (first build-results)))))))
      (testing "fs is pruned"
        (let [files-before (doall (file-seq (File. (workspace/build-path workspace))))
              build-results (pipeline/build! workspace ::basis [])
              files-after (doall (file-seq (File. (workspace/build-path workspace))))]
          (is (> (count files-before) (count files-after))))))))

(deftest build-multi-test
  (testing "invokes build-fns correctly for multiple inter-dependant build-targets"
    (with-clean-system
      (let [build-fn-calls (atom 0)
            called!        #(swap! build-fn-calls inc)
            dep-1          (make-asserting-build-target workspace ::basis "1" called! {})
            dep-2          (make-asserting-build-target workspace ::basis "2" called! {})
            dep-3          (make-asserting-build-target workspace ::basis "3" called!
                             {(:resource dep-2) (:resource dep-2)} dep-2)
            build-targets  [(make-asserting-build-target workspace ::basis "4" called!
                              {(:resource dep-1) (:resource dep-1)
                               (:resource dep-3) (:resource dep-3)}
                              dep-1 dep-3)]
            build-results  (pipeline/build! workspace ::basis build-targets)]
        (is (= 4 @build-fn-calls))
        (is (= #{"1" "2" "3" "4"} (set (map content build-results))))))))

(deftest make-protobuf-build-target-test
  (with-clean-system
    (let [tile-set-target (make-asserting-build-target workspace ::basis "1" nil {})
          material-target (make-asserting-build-target workspace ::basis "2" nil {})
          sprite-target   (pipeline/make-protobuf-build-target 3
                            (workspace/file-resource workspace "/dir/test.sprite")
                            [tile-set-target material-target]
                            Sprite$SpriteDesc
                            {:tile-set          (-> tile-set-target :resource :resource)
                             :default-animation "gurka"
                             :material          (-> material-target :resource :resource)}
                            [:tile-set :material])]
      (testing "produces correct build-target"
        (is (= 3 (:node-id sprite-target)))
        (is (= (set (:deps sprite-target)) #{tile-set-target material-target})))
      (testing "produces correct build content"
        (let [build-results (pipeline/build! workspace ::basis [sprite-target])
              sprite-result (first (filter #(= (:resource %) (:resource sprite-target)) build-results))
              pb-data (protobuf/bytes->map Sprite$SpriteDesc (content-bytes sprite-result))]
          ;; assert resource paths have been resolved to build paths
          (is (= {:tile-set (-> tile-set-target :resource resource/proj-path)
                  :default-animation "gurka"
                  :material (-> material-target :resource resource/proj-path)}
                (select-keys pb-data [:tile-set :default-animation :material]))))))))
