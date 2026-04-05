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
  (:require [dynamo.graph :as g]))

(set! *warn-on-reflection* true)

(g/defnode EffectLogNode
  (property effect-log g/Any (default [])))

(defn effect-log-setter [prop-kw]
  {:pre [(keyword? prop-kw)]}
  (fn effect-log-property-set-fn [_evaluation-context self old-value new-value]
    (let [log-entry {:prop-kw prop-kw
                     :old-value old-value
                     :new-value new-value}]
      (g/update-property self :effect-log conj log-entry))))

(defn effect-log [effect-log-node-id]
  {:pre [(g/node-instance? EffectLogNode effect-log-node-id)]}
  (g/valid-node-value effect-log-node-id :effect-log))
