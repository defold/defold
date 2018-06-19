(ns util.text-util
  (:require [clojure.java.io :as io]
            [clojure.string :as string])
  (:import (java.io BufferedReader Reader)))

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

(defn binary?
  "Tries to guess whether some readable thing has binary content. Looks at the
  first `chars-to-check` number of chars from readable and if more than
  `binary-chars-threshold` of them are not `text-char?` then consider the
  content binary."
  ([readable]
   (binary? readable nil))
  ([readable {:keys [chars-to-check binary-chars-threshold]
              :or   {chars-to-check 1000
                     binary-chars-threshold 0.01}}]
   (with-open [rdr (io/reader readable)]
     (let [cbuf (char-array chars-to-check)
           nread (.read rdr cbuf 0 chars-to-check)
           limit (* binary-chars-threshold nread)]
       (loop [i 0
              binary-chars 0]
         (if (< i nread)
           (if (< limit binary-chars)
             true
             (recur (inc i) (if (text-char? (aget cbuf i)) binary-chars (inc binary-chars))))
           false))))))

(defn- consume-separator [^String str start-index]
  (loop [quote-char-count 0
         index start-index]
    (if (= index (count str))
      [index \,]
      (let [char (.charAt str index)]
        (cond
          (= \" char)
          (recur (inc quote-char-count) (inc index))

          (or (= \, char) (Character/isWhitespace char))
          (recur quote-char-count (inc index))

          :else
          (let [quoted? (odd? quote-char-count)
                stop-char (if quoted? \" \,)]
            [index stop-char]))))))

(defn parse-comma-separated-string
  "Parses a string of comma-separated, optionally quoted strings into a vector of unquoted strings."
  [^String str]
  (let [[start-index stop-char] (consume-separator str 0)]
    (loop [substrings (transient [])
           builder (StringBuilder.)
           index start-index
           stop-char stop-char]
      (if (= index (count str))
        (persistent!
          (if (zero? (.length builder))
            substrings
            (conj! substrings (.toString builder))))
        (let [char (.charAt str index)]
          (if (= char stop-char)
            (let [[next-index next-stop-char] (consume-separator str (inc index))]
              (recur (conj! substrings (.toString builder))
                     (StringBuilder.)
                     next-index
                     next-stop-char))
            (recur substrings
                   (.append builder char)
                   (inc index)
                   stop-char)))))))

(defn join-comma-separated-string
  ^String [strings]
  (let [inner-string (string/join "\", \"" (remove empty? strings))]
    (if (empty? inner-string)
      inner-string
      (str "\"" inner-string "\""))))
