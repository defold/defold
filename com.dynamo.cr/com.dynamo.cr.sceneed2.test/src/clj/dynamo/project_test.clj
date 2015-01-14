(ns dynamo.project-test
  (:require [clojure.test :refer :all]
            [dynamo.node :as n]
            [dynamo.project :as p]
            [dynamo.file :as f]
            [dynamo.system :as ds]
            [dynamo.system.test-support :refer [with-clean-world]]
            [clojure.java.io :as io])
  (:import [java.io StringReader]))

(n/defnode DummyNode)

(defrecord ExtensionHolder [ext]
  f/PathManipulation
  (extension [this] ext)

  io/IOFactory
  (make-reader [this opts] (StringReader. "")))

(deftest load-resource
  (testing "throws if return value is not a node"
    (with-clean-world
      (let [project-node (ds/transactional
                           (ds/in (ds/add (n/construct p/Project))
                             (p/register-loader "dummy" (constantly :not-a-node))
                             (ds/current-scope)))]
        (is (thrown-with-msg? AssertionError #"must return a node"
              (p/load-resource project-node (ExtensionHolder. "dummy"))))))))

(deftest make-editor
  (testing "throws if return value is not a node"
    (with-clean-world
      (let [project-node (ds/transactional
                           (ds/in (ds/add (n/construct p/Project))
                             (p/register-loader "dummy" (fn [& _] (n/construct DummyNode)))
                             (p/register-editor "dummy" (constantly :not-a-node))
                             (ds/current-scope)))]
        (is (thrown-with-msg? AssertionError #"must return a node"
              (p/make-editor project-node (ExtensionHolder. "dummy") nil)))))))
