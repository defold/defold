(ns dynamo.integration.override-test-support
  (:require [dynamo.graph :as g]))

(g/defnode ReloadNode
  (property my-value g/Str))
