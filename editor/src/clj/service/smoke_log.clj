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

(ns service.smoke-log
  "Logger for logging extra events for interaction with the smoke test robot.
  Enable this logger by passing -Ddefold.smoke.log=true as a JVM parameter."
  (:import [org.slf4j LoggerFactory Logger]))

(set! *warn-on-reflection* true)

(def ^:constant ^:private enabled? (delay (Boolean/valueOf (System/getProperty "defold.smoke.log"))))

(defn smoke-log
  [^String message]
  (when @enabled?
    (let [logger (LoggerFactory/getLogger "DEFOLD-SMOKE-LOG")]
      (.info logger message))))

