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

(ns util.diff
  (:require [editor.code.util :as code.util]
            [util.coll :refer [pair]]
            [util.fn :as fn])
  (:import [org.eclipse.jgit.diff Edit Edit$Type EditList HistogramDiff RawText RawTextComparator]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- edit-type->keyword [^Edit$Type edit-type]
  (condp = edit-type
    Edit$Type/INSERT :insert
    Edit$Type/DELETE :delete
    Edit$Type/REPLACE :replace
    Edit$Type/EMPTY :empty))

(defn- edit->map [^Edit edit]
  {:type (edit-type->keyword (.getType edit))
   :left {:begin (.getBeginA edit)
          :end (.getEndA edit)}
   :right {:begin (.getBeginB edit)
           :end (.getEndB edit)}})

(definline nop-edit? [edit]
  `(= :nop (:type ~edit)))

(defn- redundant-edit? [edit]
  (and (nop-edit? edit)
       (let [{:keys [left right]} edit]
         (= (:begin left) (:end left))
         (= (:begin right) (:end right)))))

(defn- histogram-diff-strings
  ^EditList [^String left-string ^String right-string]
  (.diff (HistogramDiff.)
         RawTextComparator/DEFAULT
         (RawText. (.getBytes left-string))
         (RawText. (.getBytes right-string))))

(defn- cut-edit [edit cutter-edit]
  [(-> edit
       (assoc-in [:left :end]  (get-in cutter-edit [:left :begin]))
       (assoc-in [:right :end] (get-in cutter-edit [:right :begin])))
   cutter-edit
   (-> edit
       (assoc-in [:left :begin] (get-in cutter-edit [:left :end]))
       (assoc-in [:right :begin] (get-in cutter-edit [:right :end])))])

(defn- insert-nop-edits [significant-edits full-document-nop-edit]
  {:pre [(nop-edit? full-document-nop-edit)]}
  (let [[nop-edit edits]
        (reduce (fn [[nop-edit edits] edit]
                  (let [[a b c] (cut-edit nop-edit edit)]
                    (pair c (-> edits (conj! a) (conj! b)))))
                (pair full-document-nop-edit (transient []))
                significant-edits)]
    (persistent!
      (conj! edits nop-edit))))

(defn- lines->edit-end
  ^long [lines]
  ;; For the purpose of diff edits, the newline character is considered to
  ;; belong to the end of the line it terminates.
  (let [line-count (count lines)]
    (if (= "" (peek lines))
      (dec line-count)
      line-count)))

(defn find-edits [^String left-string ^String right-string]
  ;; NOTE: CRLF characters are expected to have been converted to LF in strings.
  (let [significant-edits (map edit->map (histogram-diff-strings left-string right-string))
        left-lines (code.util/split-lines left-string)
        right-lines (code.util/split-lines right-string)
        left-end (lines->edit-end left-lines)
        right-end (lines->edit-end right-lines)
        full-document-nop-edit {:type :nop
                                :left {:begin 0 :end left-end}
                                :right {:begin 0 :end right-end}}
        edits (into []
                    (remove redundant-edit?)
                    (insert-nop-edits significant-edits
                                      full-document-nop-edit))]
    {:left-lines left-lines
     :right-lines right-lines
     :edits edits}))

(defn- make-snip-line-string-raw
  "Constructs a string that will indicate a section of omitted lines in the diff
  output. The string is in the format `..... / .....`, and the number of dots on
  each side of the slash is determined by the column width required for the line
  numbers on the left side of the output so that the slash character aligns with
  the + and - indicators for the modified lines."
  ^String [^long line-number-fmt-width]
  (let [dots-width (inc (* 2 line-number-fmt-width))
        string-builder (StringBuilder. (int (+ 3 (* 2 dots-width))))]
    (loop [i 0]
      (when (< i dots-width)
        (.append string-builder \.)
        (recur (inc i))))
    (let [dots-string (.toString string-builder)]
      (.append string-builder " / ")
      (.append string-builder dots-string)
      (.toString string-builder))))

(def ^:private ^String make-snip-line-string (fn/memoize make-snip-line-string-raw))

(defn- make-diff-output-lines-impl [left-lines right-lines edits ^long context-line-limit]
  (let [last-edit-index (dec (count edits))
        line-number-fmt-width (inc (int (Math/log10 (double (max 1 (count left-lines) (count right-lines))))))
        unmodified-line-fmt (str "%" line-number-fmt-width "d %" line-number-fmt-width "d   %s")
        deleted-line-fmt (str "%" line-number-fmt-width "d %" line-number-fmt-width "s - %s")
        inserted-line-fmt (str "%" line-number-fmt-width "s %" line-number-fmt-width "d + %s")
        snip-line-string (make-snip-line-string line-number-fmt-width)
        snip-lines [snip-line-string]

        unmodified-line
        (fn unmodified-line [^long left-row ^long right-row]
          (format unmodified-line-fmt (inc left-row) (inc right-row) (right-lines right-row)))

        deleted-line
        (fn deleted-line [^long left-row]
          (format deleted-line-fmt (inc left-row) "" (left-lines left-row)))

        inserted-line
        (fn inserted-line [^long right-row]
          (format inserted-line-fmt "" (inc right-row) (right-lines right-row)))]

    (into []
          (comp
            (map-indexed
              (fn [^long edit-index {:keys [type left right]}]
                (case type
                  :nop
                  (let [^long left-begin (:begin left)
                        ^long left-end (:end left)
                        ^long right-begin (:begin right)
                        ^long right-end (:end right)
                        is-first-edit (zero? edit-index)
                        is-last-edit (== last-edit-index edit-index)
                        line-count (- left-end left-begin)
                        line-limit (if (or is-first-edit is-last-edit)
                                     context-line-limit
                                     (* 2 context-line-limit))]
                    (assert (= line-count (- right-end right-begin)))
                    (cond
                      (or (neg? context-line-limit)
                          (<= line-count line-limit)) ; No trimming required.
                      (map unmodified-line
                           (range left-begin left-end)
                           (range right-begin right-end))

                      is-first-edit ; Trim away unmodified lines from the start. The line numbers are identical here.
                      (let [trimmed-left-begin (max left-begin (- left-end context-line-limit))
                            trimmed-range (range trimmed-left-begin left-end)
                            lines (map unmodified-line
                                       trimmed-range
                                       trimmed-range)]
                        lines)

                      is-last-edit ; Trim away unmodified lines from the end.
                      (let [trimmed-left-end (min left-end (+ left-begin context-line-limit))
                            trimmed-right-end (min right-end (+ right-begin context-line-limit))
                            lines (map unmodified-line
                                       (range left-begin trimmed-left-end)
                                       (range right-begin trimmed-right-end))]
                        lines)

                      :else ; Middle edit. Trim away unmodified lines from the middle and insert a snip marker.
                      (let [trimmed-top-left-end (min left-end (+ left-begin context-line-limit))
                            trimmed-top-right-end (min right-end (+ right-begin context-line-limit))
                            trimmed-bottom-left-begin (max left-begin (- left-end context-line-limit))
                            trimmed-bottom-right-begin (max right-begin (- right-end context-line-limit))
                            top-lines (map unmodified-line
                                           (range left-begin trimmed-top-left-end)
                                           (range right-begin trimmed-top-right-end))
                            bottom-lines (map unmodified-line
                                              (range trimmed-bottom-left-begin left-end)
                                              (range trimmed-bottom-right-begin right-end))]
                        (concat top-lines snip-lines bottom-lines))))

                  :delete
                  (map deleted-line
                       (range (:begin left) (:end left)))

                  :insert
                  (map inserted-line
                       (range (:begin right) (:end right)))

                  :replace
                  (concat
                    (map deleted-line
                         (range (:begin left) (:end left)))
                    (map inserted-line
                         (range (:begin right) (:end right)))))))
            cat)
          edits)))

(defn make-diff-output-lines
  "Performs a line diff on the input strings and returns a vector of lines
  highlighting the differences, or nil if the input strings are identical. If
  context-line-limit is positive, we will limit the number of lines surrounding
  each change in the output and insert snip markers in the output where lines
  are omitted."
  [^String left-string ^String right-string ^long context-line-limit]
  (let [{:keys [left-lines right-lines edits]} (find-edits left-string right-string)]
    (when-not (every? nop-edit? edits)
      (make-diff-output-lines-impl left-lines right-lines edits context-line-limit))))
