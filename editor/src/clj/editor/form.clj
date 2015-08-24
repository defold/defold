(ns editor.form
  (:require [dynamo.graph :as g]))

(defn set-value! [{:keys [user-data set]} path value]
  (set user-data path value))

(defn clear-value! [{:keys [user-data clear]} path]
  (clear user-data path))
