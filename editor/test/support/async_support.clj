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

(ns support.async-support
  (:require [clojure.core.async :as a]))

(set! *warn-on-reflection* true)

(defn eventually
  "Wait to take 1 value from the channel, failing on timeout (default 10 sec)"
  ([ch]
   (eventually ch (* 10 1000)))
  ([ch timeout-ms]
   (a/alt!!
     ch ([v] v)
     (a/timeout timeout-ms) (throw (ex-info "Timeout" {})))))