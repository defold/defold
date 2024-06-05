(ns editor.protobuf-forms-util
  (:require [dynamo.graph :as g]))

(defn set-form-op [{:keys [node-id]} [property] value]
  (g/set-property! node-id property value))

(defn clear-form-op [{:keys [node-id]} [property]]
  (g/clear-property! node-id property))
