(ns editor.defold-project-test
  (:require [clojure.test :refer :all]
            [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :refer [with-clean-system tx-nodes]]))

(def ^:private load-counter (atom 0))

(g/defnode ANode
  (inherits resource-node/ResourceNode)
  (property value-piece g/Str)
  (property value g/Str
            (set (fn [evaluation-context self old-value new-value]
                   (let [input (g/node-value self :value-input evaluation-context)]
                     (g/set-property self :value-piece (str (first input)))))))
  (input value-input g/Str))

(g/defnode BNode
  (inherits resource-node/ResourceNode)
  (property value g/Str))

(defn- load-a [project self resource]
  (swap! load-counter inc)
  (let [data (read-string (slurp resource))]
    (concat
     (g/set-property self :value-piece "set incorrectly")
     (project/connect-resource-node project (:b data) self [[:value :value-input]])
     (g/set-property self :value "bogus value"))))

(defn- load-b [project self resource]
  (swap! load-counter inc)
  (let [data (read-string (slurp resource))]
    (g/set-property self :value (:value data))))

(defn- register-resource-types [workspace types]
  (for [type types]
    (apply workspace/register-resource-type workspace (flatten (vec type)))))

(deftest loading
  (reset! load-counter 0)
  (with-clean-system
    (let [workspace (workspace/make-workspace world (.getAbsolutePath (io/file "test/resources/load_project")))]
      (g/transact
       (register-resource-types workspace [{:ext "type_a"
                                            :node-type ANode
                                            :load-fn load-a
                                            :label "Type A"}
                                           {:ext "type_b"
                                            :node-type BNode
                                            :load-fn load-b
                                            :label "Type B"}]))
      (workspace/resource-sync! workspace)
      (let [project (test-util/setup-project! workspace)
            a1 (project/get-resource-node project "/a1.type_a")]
        (is (= 3 @load-counter))
        (is (= "t" (g/node-value a1 :value-piece)))))))
