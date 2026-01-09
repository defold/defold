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

(ns util.time
  (:require [editor.localization :as localization])
  (:import (java.time Duration Instant LocalDateTime ZoneId)
           (java.time.format DateTimeParseException)))

(defn now
  "Returns this instant."
  ^Instant []
  (Instant/now))

(defn between
  "Returns the duration of time passed between then and now."
  ^Duration [^Instant then ^Instant now]
  (Duration/between then now))

(defn since
  "Returns the duration of time passed since then."
  ^Duration [^Instant then]
  (between then (now)))

(defn vague-description
  "Returns a relaxed description of how an Instant relates to the current time.
  Typically used when formatting timestamps for UI purposes."
  ^String [^Instant instant]
  (let [now (LocalDateTime/now)
        then (LocalDateTime/ofInstant instant (ZoneId/systemDefault))
        elapsed (Duration/between then now)]
    (condp > (.getSeconds elapsed)
      45 (localization/message "time.vague.just-now")
      89  (localization/message "time.vague.a-minute-ago")
      149 (localization/message "time.vague.a-few-minutes-ago")
      899 (localization/message "time.vague.minutes-ago" {"count" (.toMinutes elapsed)})
      2699 (localization/message "time.vague.half-an-hour-ago")
      5399 (localization/message "time.vague.an-hour-ago")
      8999 (localization/message "time.vague.a-few-hours-ago")
      64799 (localization/message "time.vague.hours-ago" {"count" (.toHours elapsed)})
      129599 (localization/message "time.vague.a-day-ago")
      (localization/date (.toLocalDate then)))))

(defn try-parse-instant
  "Parses an Instant from a String. The string is expected to be in the format
  produced by calling the .toString method of the Instant class. Returns nil if
  the timestamp string is nil or not in the expected format."
  ^Instant [^String timestamp]
  (when (some? timestamp)
    (try
      (Instant/parse timestamp)
      (catch DateTimeParseException _
        nil))))
