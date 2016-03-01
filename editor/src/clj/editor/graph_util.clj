(ns editor.graph-util
  (:require [dynamo.graph :as g]))

(set! *warn-on-reflection* true)

(defmacro passthrough [field]
  `(g/fnk [~field] ~field))
