(ns editor.workspace-test
  (:require [clojure.test :refer :all]
            [editor
             [workspace :as workspace]]))

(deftest filter-resource-tree-test
  (is (= [1 {:a 1 :children [{:b 1}]}]
         (workspace/filter-resource-tree {:a 1 :children [{:b 1}]} #{{:b 1}})))
  (is (= [2 {:a 1, :children [{:b 1, :children [{:z 1}]} {:d 1, :children [{:z 1}]}]}]
         (workspace/filter-resource-tree {:a 1 :children [{:b 1 :children [{:z 1}]}
                                                          {:c 1 :children [{:y 1}]}
                                                          {:d 1 :children [{:z 1}]}]}
                                         #{{:z 1}})))
  (is (= nil
         (workspace/filter-resource-tree {:a 1 :children [{:b 1}]} #{{:c 1}})))
  (is (= [2 {:a 1, :children [{:b 1} {:b 1}]}]
         (workspace/filter-resource-tree {:a 1 :children [{:b 1} {:c 1} {:b 1}]} #{{:b 1}})))
  (is (= [1 {:a 1, :children [{:b 1, :children [{:c 1}]}]}])
      (workspace/filter-resource-tree {:a 1 :children [{:b 1 :children [{:c 1}]}]} #{{:c 1}}))
  (is (= [1 {:a 1, :children [{:l 1, :children [{:m 1, :children [{:n 1, :children [{:b 1}]}]}]}]}]
         (workspace/filter-resource-tree {:a 1 :children [{:l 1 :children [{:m 1 :children [{:n 1 :children [{:b 1}]}]}]}]}
                                         #{{:b 1}}))))
