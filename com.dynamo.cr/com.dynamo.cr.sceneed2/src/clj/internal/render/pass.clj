(ns internal.render.pass
  (:require [dynamo.types :as t]))

(defrecord RenderPass [selection model-transform]
  t/Pass
  (t/selection? [this] selection)
  (t/model-transform? [this] model-transform))

(defmacro make-pass [lbl sel model-xfm]
  `(def ~lbl (RenderPass. ~sel ~model-xfm)))

(defmacro make-passes [& forms]
  (let [ps (partition 3 forms)]
    `(do
       ~@(map #(list* 'make-pass %) ps)
       (def selection-passes ~(mapv first (filter second ps)))
       (def passes           ~(mapv first ps)))))

(make-passes
  background     false false
  opaque         false true
  transparent    false true
  icon-outline   false false
  outline        false true
  manipulator    false true
  overlay        false false
  selection      true  true
  icon           false false
  icon-selection true false)

(doseq [[v doc]
        {*ns*
         "Render passes organize rendering into layers."

         #'selection-passes
         "Vector of the passes that participate in selection"

         #'passes
         "Vector of all passes, ordered from back to front."}]
  (alter-meta! v assoc :doc doc))
