;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns internal.txsteps.helpers
  (:require [dynamo.graph :as g]
            [util.fn :as fn]))

(set! *warn-on-reflection* true)

(g/defnode EffectLogNode
  (property effect-log-property g/Any (default [])))

(def effect-log-node-init-props
  {:effect-log-property []})

(def effect-log-node-init-props-fn
  ;; Supply as :init-props-fn to g/override to ensure the effect log is not
  ;; inherited from the original when appending entries.
  (constantly effect-log-node-init-props))

(def effect-log-node-override-opts
  {:init-props-fn effect-log-node-init-props-fn})

(defn log-effect [node-id prop-kw old-value new-value]
  (let [log-entry {:prop-kw prop-kw
                   :old-value old-value
                   :new-value new-value}]
    (g/update-property node-id :effect-log-property conj log-entry)))

(defn effect-log-setter [prop-kw]
  {:pre [(keyword? prop-kw)]}
  (fn effect-log-property-set-fn [_evaluation-context self old-value new-value]
    (log-effect self prop-kw old-value new-value)))

(defn effect-log
  ([effect-log-node-id]
   (effect-log (g/now) effect-log-node-id))
  ([basis effect-log-node-id]
   {:pre [(g/node-instance? basis EffectLogNode effect-log-node-id)]}
   (g/raw-property-value basis effect-log-node-id :effect-log-property)))

(def ^:dynamic property-test-node-setter-check-fn fn/constantly-nil)

(g/defnode PropertyTestNode
  (inherits EffectLogNode)

  (output effect-log-output g/Any :cached
          (g/fnk [effect-log-property] effect-log-property))

  (property basic-property g/Keyword)
  (output basic-output g/Keyword :cached
          (g/fnk [basic-property] basic-property))

  (property effecting-property g/Keyword
            (set (fn [evaluation-context self old-value new-value]
                   (property-test-node-setter-check-fn evaluation-context self old-value new-value)
                   (log-effect self :effecting-property old-value new-value))))
  (output effecting-output g/Keyword :cached
          (g/fnk [effecting-property] effecting-property)))

(g/defnode ConnectionSourceNode
  (property value g/Keyword)
  (output value-output g/Keyword :cached
          (g/fnk [value] value)))

(g/defnode ConnectionTargetNode
  (input regular-input g/Keyword)
  (output regular-output g/Keyword :cached
          (g/fnk [regular-input] regular-input))

  (input array-input g/Keyword :array)
  (output array-output [g/Keyword] :cached
          (g/fnk [array-input] array-input))

  (input regular-cascade-delete-input g/Any :cascade-delete)
  (output regular-cascade-delete-output g/Any :cached
          (g/fnk [regular-cascade-delete-input] regular-cascade-delete-input))

  (input array-cascade-delete-input g/Any :array :cascade-delete)
  (output array-cascade-delete-output g/Any :cached
          (g/fnk [array-cascade-delete-input] array-cascade-delete-input)))
