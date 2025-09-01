(ns integration.refs-and-deps-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.resource :as resource]
            [editor.resource-dialog :as resource-dialog]
            [integration.test-util :as test-util]))

(deftest refs-and-deps-symmetry-test
  (test-util/with-loaded-project
    (let [dep-map (into {}
                        (map (fn [res]
                               (let [path (resource/proj-path res)]
                                 [path {:refs (into #{} (map resource/proj-path) (resource-dialog/refs-filter-fn project path nil))
                                        :deps (into #{} (map resource/proj-path) (resource-dialog/deps-filter-fn project path nil))}])))
                        (g/node-value workspace :resource-list))]
      (doseq [[path {:keys [refs deps]}] dep-map]
        (testing path
          (doseq [dep deps]
            (testing (str "dep on " dep)
              (is (contains? (:refs (dep-map dep)) path) (str path " depends on " dep ", but " dep " is not referenced by " path))))
          (doseq [ref refs]
            (testing (str "ref by "ref)
              (is (contains? (:deps (dep-map ref)) path) (str path " is referenced by " ref ", but " ref " does not depend on " path)))))))))
