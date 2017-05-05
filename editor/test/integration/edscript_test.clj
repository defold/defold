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
            [editor.handler :as handler]
            [editor.menu :as menu]
            [support.test-support :refer [tx-nodes with-clean-system]]
            [integration.test-util :as test-util]))

(deftest edscript-test
  (with-clean-system
    (let [workspace   (test-util/setup-workspace! world "test/resources/ed_ext")
          project     (test-util/setup-project! workspace)
          script-node (project/get-resource-node project "/test.edscript")
          handlers    (g/node-value project :user-handlers)
          context (handler/->context :global {})]
      (handler/set-user-handlers! handlers)
      (prn (first (g/node-value project :user-menus)))
      (prn (first @menu/*menus*))
      #_(test-util/handler-run :my-magic-command [context] {}))))

(edscript-test)
