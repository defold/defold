(ns util.text-util
  (:require [clojure.string :as string])
  (:import (java.io Reader)))

(defn text-char?
  "Returns true if the supplied character is textual. This can
  be used to determine if we're dealing with binary data."
  [^Character char]
  (or (Character/isWhitespace char)
      (not (Character/isISOControl char))))

(defn guess-line-endings
  "Reads character from the supplied fresh reader until it can determine the
  type of line ending used in the text. If reader is nil, does not contain line
  endings, or the data looks like it might be a binary file, returns nil. Will
  close the reader when done."
  [^Reader reader]
  (when (some? reader)
    (with-open [_ reader]
      (loop [ch (.read reader)
             cr-count 0
             lf-count 0]
        (cond
          ; If we've reached the end or seen enough, return result.
          (or (= -1 ch) (< 250 (max cr-count lf-count)))
          (if (> cr-count (/ lf-count 2))
            :crlf
            :lf)

          (= 0x0D ch) ; CR - count and recur.
          (recur (.read reader) (inc cr-count) lf-count)

          (= 0x0A ch) ; LF - count and recur.
          (recur (.read reader) cr-count (inc lf-count))

          (not (text-char? (char ch))) ; This looks like binary! Return nil.
          nil

          :else
          (recur (.read reader) cr-count lf-count))))))

(defn lf->crlf
  "Converts all LF line endings in the input string to CRLF.
  Returns nil if the input is nil."
  [^String text]
  (some-> text (string/replace #"\r\n|\n" "\r\n")))
