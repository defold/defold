(ns integration.script-properties-test
  (:require [clojure.test :refer :all]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system tx-nodes]]
            [integration.test-util :as tu]
            [editor.workspace :as workspace]
            [editor.defold-project :as project]))

(defn- component [go-id id]
  (let [comps (->> (g/node-value go-id :node-outline)
                :children
                (map (fn [v] [(-> (:label v)
                                (string/split #" ")
                                first) (:node-id v)]))
                (into {}))]
    (comps id)))

(defn- source [script-id]
  (tu/prop script-id :code))

(defn- source! [script-id source]
  (tu/prop! script-id :code source))

(defmacro with-source [script-id source & body]
  `(let [orig# (source ~script-id)]
     (source! ~script-id ~source)
     ~@body
     (source! ~script-id orig#)))

(deftest script-properties-source
  (with-clean-system
    (let [workspace (tu/setup-workspace! world)
          project (tu/setup-project! workspace)
          script-id (tu/resource-node project "/script/props.script")]
      (testing "reading values"
               (is (= 1.0 (tu/prop script-id :number)))
               (is (tu/prop-read-only? script-id :number)))
      (testing "broken prop defs" ;; string vals are not supported
               (with-source script-id "go.property(\"number\", \"my_string\")\n"
                 (is (nil? (tu/prop script-id :number))))))))

(deftest script-properties-component
  (with-clean-system
    (let [workspace (tu/setup-workspace! world)
          project (tu/setup-project! workspace)
          go-id (tu/resource-node project "/game_object/props.go")
          script-c (component go-id "script")]
      (is (= 2.0 (tu/prop script-c :number)))
      (tu/prop! (tu/prop-node-id script-c :number) :number 3.0)
      (is (= 3.0 (tu/prop script-c :number)))
      (tu/prop-clear! (tu/prop-node-id script-c :number) :number)
      (is (= 1.0 (tu/prop script-c :number))))))
