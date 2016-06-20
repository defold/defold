(ns dynamo.integration.node-become-support
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system tx-nodes]]))

(g/defnode NodeA
  (input input-a g/Str)
  (output output-a g/Str (g/always "a")))

(g/defnode NodeB
  (input input-b g/Str)
  (output output-b g/Str (g/fnk [input-b] input-b)))
