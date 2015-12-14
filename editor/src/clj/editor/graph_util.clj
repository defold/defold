(ns editor.graph-util
  (:require [dynamo.graph :as g]))

(defmacro passthrough [field]
  `(g/fnk [~field] ~field))

