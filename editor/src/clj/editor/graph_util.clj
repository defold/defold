(ns editor.graph-util
  (:require [dynamo.graph :as g]))

(defmacro proxy-set [field]
  (let [field-kw (keyword field)]
    (fn [basis self _ new-value]
      (g/set-property self field-kw new-value))))

(defmacro proxy-value [field]
  `(g/fnk [~field] ~field))

