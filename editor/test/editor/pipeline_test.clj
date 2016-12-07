(ns editor.pipeline-test
  (:require
   [clojure.test :refer [deftest testing is are]]
   [clojure.java.io :as io]
   [integration.test-util :as test-util]
   [editor.pipeline :as pipeline]
   [editor.protobuf :as protobuf]
   [editor.resource :as resource]
   [editor.workspace :as workspace])
  (:import
   (editor.resource FileResource)
   (com.dynamo.sprite.proto Sprite$SpriteDesc Sprite$SpriteDesc$BlendMode)))

(defn make-file-resource
  [path]
  (FileResource. nil "tmp" (io/file path) nil))

(defn- make-asserting-build-target
  [basis id callback dep-resources & deps]
  (let [build-resource (workspace/make-build-resource (make-file-resource (str "resource-" id)))
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
                   :content id})
     :user-data user-data
     :deps      (vec deps)}))


(deftest build-test
  (testing "invokes build-fn correctly for a single build-target"
    (let [build-fn-calls (atom 0)
          called!        #(swap! build-fn-calls inc)
          build-targets  [(make-asserting-build-target ::basis 1 called! {})]
          build-results  (pipeline/build ::basis build-targets (pipeline/make-build-cache))]
      (is (= 1 @build-fn-calls))
      (is (= 1 (:content (first build-results))))))

  (testing "invokes build-fns correctly for multiple inter-dependant build-targets"
    (let [build-fn-calls (atom 0)
          called!        #(swap! build-fn-calls inc)
          dep-1          (make-asserting-build-target ::basis 1 called! {})
          dep-2          (make-asserting-build-target ::basis 2 called! {})
          dep-3          (make-asserting-build-target ::basis 3 called!
                                                      {(:resource dep-2) (:resource dep-2)} dep-2)
          build-targets  [(make-asserting-build-target
                            ::basis 4 called! {(:resource dep-1) (:resource dep-1)
                                               (:resource dep-3) (:resource dep-3)}
                            dep-1 dep-3)]
          build-results  (pipeline/build ::basis build-targets (pipeline/make-build-cache))]
      (is (= 4 @build-fn-calls))
      (is (= #{1 2 3 4} (set (map :content build-results)))))))

(deftest build-test-cache
  (testing "caches build-result"
    (let [build-fn-calls (atom 0)
          called!        #(swap! build-fn-calls inc)
          build-targets  [(make-asserting-build-target ::basis 1 called! {})]
          cache          (pipeline/make-build-cache)
          build-result-1 (pipeline/build ::basis build-targets cache)
          build-result-2 (pipeline/build ::basis build-targets cache)]
      (is (=  1 @build-fn-calls))))

  (testing "does not use cached result if produced by incompatible build-target"
    (let [build-fn-calls (atom 0)
          called!        #(swap! build-fn-calls inc)
          build-target   (make-asserting-build-target ::basis 1 called! {})
          build-targets  [build-target]
          cache          (pipeline/make-build-cache)
          _              (pipeline/build ::basis build-targets cache)
          initial-cache  @cache]
      (pipeline/build ::basis build-targets cache)
      (is (= 1 @build-fn-calls))

      (testing "does not use cache if source resource is different"
        (let [cache (pipeline/make-build-cache (update-in initial-cache [(:resource build-target) :key]
                                                          (fn [[source-resource build-fn user-data]]
                                                            ["different-source-resource" build-fn user-data])))]
          (pipeline/build ::basis build-targets cache)
          (pipeline/build ::basis build-targets cache)
          (is (= 2 @build-fn-calls))))

      (testing "does not use cache if build-fn is different"
        (let [cache (pipeline/make-build-cache (update-in initial-cache [(:resource build-target) :key]
                                                          (fn [[source-resource build-fn user-data]]
                                                            [source-resource (constantly 42) user-data])))]
          (pipeline/build ::basis build-targets cache)
          (pipeline/build ::basis build-targets cache)
          (is (= 3 @build-fn-calls))))

      (testing "does not use cache if user-data is different"
        (let [cache (pipeline/make-build-cache (update-in initial-cache [(:resource build-target) :key]
                                                          (fn [[source-resource build-fn user-data]]
                                                            [source-resource build-fn {:gurka "42"}])))]
          (pipeline/build ::basis build-targets cache)
          (pipeline/build ::basis build-targets cache)
          (is (= 4 @build-fn-calls)))))))

(deftest make-protobuf-build-target-test
  (let [tile-set-target (make-asserting-build-target ::basis 1 nil {})
        material-target (make-asserting-build-target ::basis 2 nil {})
        sprite-target   (pipeline/make-protobuf-build-target 3
                                                             (make-file-resource "/test.sprite")
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
      (let [build-results (pipeline/build ::basis [sprite-target] (pipeline/make-build-cache))
            sprite-result (first (filter #(= (:resource %) (:resource sprite-target)) build-results))
            pb-data (protobuf/bytes->map Sprite$SpriteDesc (:content sprite-result))]
        ;; assert resource paths have been resolved to build paths
        (is (= {:tile-set (-> tile-set-target :resource resource/proj-path)
                :default-animation "gurka"
                :material (-> material-target :resource resource/proj-path)}
               (select-keys pb-data [:tile-set :default-animation :material])))))))
