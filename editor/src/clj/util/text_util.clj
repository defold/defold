(ns util.text-util
  (:require [clojure.java.io :as io]
            [clojure.string :as string])
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
             prev-ch nil
             lf-count 0
             crlf-count 0]
        (cond
          ; If we've reached the end or seen enough, return result.
          (or (= -1 ch) (< 250 (max lf-count crlf-count)))
          (cond
            (= 0 lf-count crlf-count) nil
            (> lf-count crlf-count) :lf
            :else :crlf)

          (= 0x0A ch) ; LF character.
          (if (= 0x0D prev-ch) ; Previous was CR character.
            (recur (.read reader) ch lf-count (inc crlf-count))
            (recur (.read reader) ch (inc lf-count) crlf-count))

          (not (text-char? (char ch))) ; This looks like binary! Return nil.
          nil

          :else
          (recur (.read reader) ch lf-count crlf-count))))))

(defn guess-line-separator
  "Tries to guess the type of line endings used in the supplied input string and
  returns a suitable line separator string. If the type of line endings in use
  cannot be determined, defaults to a single newline character."
  ^String [^String text]
  (case (some-> text .getBytes io/reader guess-line-endings)
    :crlf "\r\n"
    "\n"))

(defn scan-line-endings
  "Reads character from the supplied fresh reader until it reaches the end of
  the file, finds mixed line endings, or detects that the file is binary. For a
  text file with consistent line endings, it will read the entire file. Returns
  the type of line ending used in the file, or :mixed if the file mixes line
  endings. If reader is nil, does not contain line endings, or the data looks
  like it might be a binary file, returns nil. Will close the reader when done."
  [^Reader reader]
  (when (some? reader)
    (with-open [_ reader]
      (loop [ch (.read reader)
             prev-ch nil
             line-ending nil]
        (cond
          (= -1 ch) ; We've reached the end, return result.
          line-ending

          (= 0x0A ch) ; LF character.
          (cond
            (= 0x0D prev-ch) ; Previous was CR character.
            (if (= :lf line-ending)
              :mixed
              (recur (.read reader) ch :crlf))

            (= :crlf line-ending)
            :mixed

            :else
            (recur (.read reader) ch :lf))

          (not (text-char? (char ch))) ; This looks like binary! Return nil.
          nil

          :else
          (recur (.read reader) ch line-ending))))))

(defn crlf->lf
  "Converts all CRLF line endings in the input string to LF.
  Broken line endings such as CRCRLF will be converted into a single LF.
  Rouge CR characters will be removed.
  Returns nil if the input is nil."
  [^String text]
  (some-> text (string/replace #"\r" "")))

(defn lf->crlf
  "Converts all LF line endings in the input string to CRLF.
  Broken line endings such as CRCRLF will be converted into a single CRLF.
  Rouge CR characters will be removed.
  Returns nil if the input is nil."
  [^String text]
  (some-> text (string/replace #"\r" "") (string/replace #"\n" "\r\n")))
