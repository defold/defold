(ns internal.render.pass)

(defprotocol Pass
  (selection?       [this])
  (model-transform? [this]))

(defrecord RenderPass [selection model-transform]
  Pass
  (selection? [this] selection)
  (model-transform? [this] model-transform))

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

         #'selection?
         "Replies true when the pass is used during pick render."

         #'model-transform?
         "Replies true when the pass should apply the node transforms to
the current model-view matrix. (Will be true in most cases, false for overlays.)"

         #'selection-passes
         "Vector of the passes that participate in selection"

         #'passes
         "Vector of all passes, ordered from back to front."}]
  (alter-meta! v assoc :doc doc))