(ns editor.project-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system]]
            [editor.file :as f]
            [editor.project :as p])
  (:import [java.io StringReader]))

(g/defnode DummyNode)

#_(deftest find-nodes-by-extension
  (with-clean-system
    (let [[project-node d1 d2 d3 d4]
          (g/tx-nodes-added
           (g/transact
            (g/make-nodes
             world
             [project-node p/Project
              d1           [DummyNode :filename (f/native-path "foo.png")]
              d2           [DummyNode :filename (f/native-path "/var/tmp/foo.png")]
              d3           [DummyNode :filename (f/native-path "foo.script")]
              d4           [DummyNode :is-a-file-node? false]]
             (g/connect d1 :self project-node :nodes)
             (g/connect d2 :self project-node :nodes)
             (g/connect d3 :self project-node :nodes)
             (g/connect d4 :self project-node :nodes))))]
      (is (= #{d1 d2}    (p/nodes-with-extensions project-node ["png"])))
      (is (= #{d3}       (p/nodes-with-extensions project-node ["script"])))
      (is (= #{d1 d2 d3} (p/nodes-with-extensions project-node ["png" "script"]))))))
