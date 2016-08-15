(ns editor.collision-groups-test
  (:require
   [clojure.test :as test :refer [deftest testing is]]
   [editor.collision-groups :as collision-groups :refer [allocate-collision-groups]]))

(deftest allocate-collision-groups-test
  (testing "allocates ids in node id order"
    (is (= (-> collision-groups/empty-state
               (allocate-collision-groups
                [{:node-id 3 :collision-group "c"}
                 {:node-id 1 :collision-group "a"}
                 {:node-id 2 :collision-group "b"}])
               :group->id)
           {"a" 0 "b" 1 "c" 2})))

  (testing "allocates same id to same group"
    (is (= (-> collision-groups/empty-state
               (allocate-collision-groups
                [{:node-id 1 :collision-group "a"}
                 {:node-id 2 :collision-group "b"}
                 {:node-id 3 :collision-group "c"}
                 {:node-id 4 :collision-group "b"}])
               :group->id)
           {"a" 0 "b" 1 "c" 2})))

  (testing "retains previously allocated ids"
    (is (= (-> collision-groups/empty-state
               (allocate-collision-groups
                [{:node-id 1 :collision-group "a"}
                 {:node-id 2 :collision-group "b"}
                 {:node-id 3 :collision-group "c"}])
               (allocate-collision-groups
                [{:node-id 1 :collision-group "a"}
                 {:node-id 2 :collision-group "b"}
                 {:node-id 3 :collision-group "c"}
                 {:node-id 4 :collision-group "d"}])
               :group->id)
           {"a" 0 "b" 1 "c" 2 "d" 3})))

  (testing "retains allocated id when all usages renamed"
    (is (= (-> collision-groups/empty-state
               (allocate-collision-groups
                [{:node-id 1 :collision-group "a"}
                 {:node-id 2 :collision-group "b"}
                 {:node-id 3 :collision-group "c"}])
               (allocate-collision-groups
                [{:node-id 1 :collision-group "a"}
                 {:node-id 2 :collision-group "d"}
                 {:node-id 3 :collision-group "c"}])
               :group->id)
           {"a" 0 "d" 1 "c" 2})))

    (testing "allocates new id when not all usages renamed"
      (is (= (-> collision-groups/empty-state
                 (allocate-collision-groups
                  [{:node-id 1 :collision-group "a"}
                   {:node-id 2 :collision-group "b"}
                   {:node-id 3 :collision-group "a"}])
                 (allocate-collision-groups
                  [{:node-id 1 :collision-group "c"}
                   {:node-id 2 :collision-group "b"}
                   {:node-id 3 :collision-group "a"}])
               :group->id)
           {"a" 0 "b" 1 "c" 2}))))
