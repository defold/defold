(ns dynamo.project-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [dynamo.file :as f]
            [dynamo.graph :as g]
            [dynamo.node :as n]
            [dynamo.project :as p]
            [dynamo.system :as ds]
            [dynamo.system.test-support :refer [with-clean-system tx-nodes]]
            [dynamo.types :as t])
  (:import [java.io StringReader]))

(g/defnode DummyNode)

(g/defnode FakeProject
  t/NamingContext
  (lookup [this name]
    (g/transactional (ds/add (n/construct DummyNode)))))

(defrecord ExtensionHolder [ext]
  t/PathManipulation
  (extension [this] ext)

  io/IOFactory
  (make-reader [this opts] (StringReader. "")))

#_(defn- build-dummy-project
  []
  (let [project-node (n/construct FakeProject)]
    (g/transactional
      (ds/add project-node)
      (ds/in project-node
        (p/register-editor "dummy" (constantly :not-a-node))
        project-node))))

#_(deftest make-editor
  (testing "throws if return value is not a node"
    (with-clean-system
      (let [project-node (build-dummy-project)]
        (is (thrown-with-msg? AssertionError #"must return a node"
              (p/make-editor project-node (ExtensionHolder. "dummy") nil)))))))

(deftest find-nodes-by-extension
  (with-clean-system
    (let [d1           (n/construct DummyNode :filename (f/native-path "foo.png"))
          d2           (n/construct DummyNode :filename (f/native-path "/var/tmp/foo.png"))
          d3           (n/construct DummyNode :filename (f/native-path "foo.script"))
          d4           (n/construct DummyNode :is-a-file-node? false)
          [project-node d1 d2 d3 d4] (g/transactional
                                         (ds/in (ds/add (n/construct p/Project))
                                           [(ds/current-scope) (ds/add d1) (ds/add d2) (ds/add d3) (ds/add d4)]))]
      (is (= #{d1 d2}    (p/nodes-with-extensions project-node ["png"])))
      (is (= #{d3}       (p/nodes-with-extensions project-node ["script"])))
      (is (= #{d1 d2 d3} (p/nodes-with-extensions project-node ["png" "script"]))))))
