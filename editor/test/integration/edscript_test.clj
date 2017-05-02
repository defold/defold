(ns integration.edscript-test
  (:require [clojure.java.io :as io]
            [clojure.string :as str]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.script :as script]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [editor.defold-project :as project]
            [editor.code-completion :refer :all]
            [support.test-support :refer [tx-nodes with-clean-system]]
            [integration.test-util :as test-util]))

(deftest edscript-test
  (with-clean-system
    (let [workspace   (test-util/setup-workspace! world "test/resources/ed_ext")
          project     (test-util/setup-project! workspace)
          script-node (project/get-resource-node project "/test.edscript")
          h           (g/node-value script-node :handlers)]
      (prn (g/node-value project :user-handlers)))))

(edscript-test)
