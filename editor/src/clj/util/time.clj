(ns util.time
  (:import (java.time Duration Instant LocalDateTime ZoneId)
           (java.time.format DateTimeParseException)))

(defn vague-description
  "Returns a relaxed description of how an Instant relates to the current time.
  Typically used when formatting timestamps for UI purposes."
  ^String [^Instant instant]
  (let [now (LocalDateTime/now)
        then (LocalDateTime/ofInstant instant (ZoneId/systemDefault))
        elapsed (Duration/between then now)]
    (condp > (.getSeconds elapsed)
      45 "just now"
      89  "a minute ago"
      149 "a few minutes ago"
      899 (str (.toMinutes elapsed) " minutes ago")
      2699 "half an hour ago"
      5399 "an hour ago"
      8999 "a few hours ago"
      64799 (str (.toHours elapsed) " hours ago")
      129599 "a day ago"
      (str (.toLocalDate then)))))

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
