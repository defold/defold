(ns dynamo.views)

(defprotocol Part
  (create [this parent] "Create controls in a new part."))