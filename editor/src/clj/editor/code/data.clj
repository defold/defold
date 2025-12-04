;; Copyright 2020-2025 The Defold Foundation
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

(ns editor.code.data
  (:require [clojure.set :as set]
            [clojure.string :as string]
            [editor.code.syntax :as syntax]
            [editor.code.util :as util]
            [util.coll :as coll :refer [pair]])
  (:import [java.io IOException InputStream Reader Writer]
           [java.nio CharBuffer]
           [java.util Collections]
           [java.util.regex MatchResult Pattern]
           [org.apache.commons.io.input ReaderInputStream]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^:private clipboard-mime-type-plain-text "text/plain")
(def ^:private clipboard-mime-type-multi-selection "application/x-string-vector")

(defprotocol Clipboard
  (has-content? [this mime-type])
  (get-content [this mime-type])
  (set-content! [this representation-by-mime-type]))

(defprotocol GlyphMetrics
  (ascent [this] "A rounded double representing the distance from the baseline to the top.")
  (line-height [this] "A rounded double representing the line height.")
  (char-width [this character] "A rounded double representing the width of the specified character."))

(defn- identifier-character-at-index? [line ^long index]
  (if-some [^char character (get line index)]
    (case character
      \_ true
      (Character/isJavaLetterOrDigit character))
    false))

(defn- whitespace-character-at-index? [line ^long index]
  (if-some [^char character (get line index)]
    (Character/isWhitespace character)
    false))

(defn- word-boundary-before-index? [line ^long index]
  (if-some [^char character (get line index)]
    (some? (re-find (re-pattern (str "\\b\\Q" character "\\E")) line))
    true))

(defn- word-boundary-after-index? [line ^long index]
  (if-some [^char character (get line (dec index))]
    (some? (re-find (re-pattern (str "\\Q" character "\\E\\b")) line))
    true))

(defn- compare-extra-fields
  ^long [declared-fields a b]
  (loop [keys (into (sorted-set) (remove declared-fields) (concat (keys a) (keys b)))]
    (if-some [key (first keys)]
      (let [value-comparison (compare (key a) (key b))]
        (if (zero? value-comparison)
          (recur (next keys))
          value-comparison))
      0)))

(def ^:private compare-extra-cursor-fields (partial compare-extra-fields #{:row :col}))

(defrecord Cursor [^long row ^long col]
  Comparable
  (compareTo [this other]
    (util/comparisons
      (compare row (.row ^Cursor other))
      (compare col (.col ^Cursor other))
      (compare-extra-cursor-fields this other))))

(defn cursor-print-data [^Cursor c]
  (into [(.-row c) (.-col c)]
        cat
        (dissoc c :row :col)))

(defmethod print-method Cursor [c, ^Writer w]
  (.write w "#code/cursor")
  (print-method (cursor-print-data c) w))

(defn read-cursor [[row col & {:as kvs}]]
  (if kvs
    `(Cursor. ~row ~col nil ~kvs)
    `(Cursor. ~row ~col)))

(defn compare-cursor-position
  ^long [^Cursor a ^Cursor b]
  (util/comparisons
    (compare (.row a) (.row b))
    (compare (.col a) (.col b))))

(defn- cursor-in-leading-whitespace? [lines ^Cursor cursor]
  (let [line (lines (.row cursor))]
    (if (and (zero? (.col cursor))
             (zero? (count line)))
      false
      (every? (partial whitespace-character-at-index? line)
              (range 0 (.col cursor))))))

(declare cursor-range-start cursor-range-end)
(def ^:private compare-extra-cursor-range-fields (partial compare-extra-fields #{:from :to}))

(defrecord CursorRange [^Cursor from ^Cursor to]
  Comparable
  (compareTo [this other]
    (util/comparisons
      (compare-cursor-position (cursor-range-start this) (cursor-range-start other))
      (compare-cursor-position (cursor-range-end this) (cursor-range-end other))
      (compare from (.from ^CursorRange other))
      (compare to (.to ^CursorRange other))
      (compare-extra-cursor-range-fields this other))))

(defn cursor-range-print-data [^CursorRange cr]
  (into [(cursor-print-data (.-from cr))
         (cursor-print-data (.-to cr))]
        cat
        (dissoc cr :from :to)))

(defmethod print-method CursorRange [^CursorRange cr, ^Writer w]
  (.write w "#code/range")
  (print-method (cursor-range-print-data cr) w))

(defn- read-cursor-range-cursor [cursor-form]
  (if (vector? cursor-form)
    (read-cursor cursor-form)
    cursor-form))

(defn read-cursor-range [[from to & {:as kvs}]]
  (if kvs
    `(CursorRange. ~(read-cursor-range-cursor from) ~(read-cursor-range-cursor to) nil ~kvs)
    `(CursorRange. ~(read-cursor-range-cursor from) ~(read-cursor-range-cursor to))))

(def document-start-cursor (->Cursor 0 0))
(def document-start-cursor-range (->CursorRange document-start-cursor document-start-cursor))

(defn document-end-cursor
  ^Cursor [lines]
  (let [row (max 0 (dec (count lines)))
        col (count (get lines row))]
    (->Cursor row col)))

(defn Cursor->CursorRange
  ^CursorRange [^Cursor cursor]
  (->CursorRange cursor cursor))

(defn CursorRange->Cursor
  ^Cursor [^CursorRange cursor-range]
  (.to cursor-range))

(defn sanitize-cursor-range
  ^CursorRange [^CursorRange cursor-range]
  (let [^Cursor from (.from cursor-range)
        ^Cursor to (.to cursor-range)]
    (->CursorRange (->Cursor (.row from) (.col from))
                   (->Cursor (.row to) (.col to)))))

(defn- min-cursor
  ^Cursor [^Cursor a ^Cursor b]
  (if (neg? (compare-cursor-position a b)) a b))

(defn- max-cursor
  ^Cursor [^Cursor a ^Cursor b]
  (if (pos? (compare-cursor-position a b)) a b))

(defn cursor-range-start
  ^Cursor [^CursorRange cursor-range]
  (min-cursor (.from cursor-range) (.to cursor-range)))

(defn cursor-range-end
  ^Cursor [^CursorRange cursor-range]
  (max-cursor (.from cursor-range) (.to cursor-range)))

(defn cursor-range-start-range
  ^CursorRange [^CursorRange cursor-range]
  (let [start (cursor-range-start cursor-range)]
    (->CursorRange start start)))

(defn cursor-range-end-range
  ^CursorRange [^CursorRange cursor-range]
  (let [end (cursor-range-end cursor-range)]
    (->CursorRange end end)))

(defn cursor-equals? [^Cursor a ^Cursor b]
  (and (= (.row a) (.row b))
       (= (.col a) (.col b))))

(defn cursor-range-equals? [^CursorRange a ^CursorRange b]
  (and (cursor-equals? (cursor-range-start a) (cursor-range-start b))
       (cursor-equals? (cursor-range-end a) (cursor-range-end b))))

(defn cursor-range-empty? [^CursorRange cursor-range]
  (= (.from cursor-range) (.to cursor-range)))

(defn cursor-range-multi-line? [^CursorRange cursor-range]
  (not (zero? (- (.row ^Cursor (.from cursor-range))
                 (.row ^Cursor (.to cursor-range))))))

(defn cursor-range-ends-before-row? [^long row ^CursorRange cursor-range]
  (> row (.row (cursor-range-end cursor-range))))

(defn cursor-range-starts-before-row? [^long row ^CursorRange cursor-range]
  (> row (.row (cursor-range-start cursor-range))))

(defn cursor-range-contains?
  "Note: Returns true when the cursor is on either edge of the cursor range.
  See also: [[cursor-range-contains-exclusive?]]"
  [^CursorRange cursor-range ^Cursor cursor]
  (let [start (cursor-range-start cursor-range)
        end (cursor-range-end cursor-range)]
    (cond (= (.row start) (.row end) (.row cursor))
          (and (<= (.col start) (.col cursor))
               (>= (.col end) (.col cursor)))

          (= (.row start) (.row cursor))
          (<= (.col start) (.col cursor))

          (= (.row end) (.row cursor))
          (>= (.col end) (.col cursor))

          :else
          (and (<= (.row start) (.row cursor))
               (>= (.row end) (.row cursor))))))

(defn cursor-range-contains-exclusive?
  "See also: [[cursor-range-contains?]]"
  [^CursorRange cursor-range ^Cursor cursor]
  (let [start (cursor-range-start cursor-range)
        end (cursor-range-end cursor-range)]
    (cond (= (.row start) (.row end) (.row cursor))
          (and (<= (.col start) (.col cursor))
               (> (.col end) (.col cursor)))

          (= (.row start) (.row cursor))
          (<= (.col start) (.col cursor))

          (= (.row end) (.row cursor))
          (> (.col end) (.col cursor))

          :else
          (and (<= (.row start) (.row cursor))
               (>= (.row end) (.row cursor))))))

(defn- cursor-before-or-same?
  "Returns true iff a's position <= b's position"
  [^Cursor a ^Cursor b]
  (not (pos? (compare-cursor-position a b))))

(defn- cursor-before?
  "Returns true iff a's position < b's position"
  [^Cursor a ^Cursor b]
  (neg? (compare-cursor-position a b)))

(defn cursor-range-differences
  "Returns 0, 1 or 2 cursor ranges that denote a difference of 2 cursor ranges

  Possible results are:
    0 ranges    if A is fully within B
    1 range     if A overlaps with B
    2 ranges    if B is fully within A"
  ^CursorRange [^CursorRange a ^CursorRange b]
  (let [a-start (cursor-range-start a)
        a-end (cursor-range-end a)
        b-start (cursor-range-start b)
        b-end (cursor-range-end b)]
    (cond
      ;; no intersection
      (or (cursor-before-or-same? b-end a-start)
          (cursor-before-or-same? a-end b-start))
      [a]

      ;; A within B
      (and (cursor-before-or-same? b-start a-start)
           (cursor-before-or-same? a-end b-end))
      []

      ;; B within A
      (and (cursor-before? a-start b-start)
           (cursor-before? b-end a-end))
      [(->CursorRange a-start b-start)
       (->CursorRange b-end a-end)]

      ;; intersection, B before A
      (cursor-before? b-end a-end)
      [(->CursorRange b-end a-end)]

      ;; intersection, A before B
      :else
      [(->CursorRange a-start b-start)])))

(defn cursor-range-intersection
  "Returns either nil if no intersection or an intersection cursor range.
  Note: returned range is never empty; the result is nil if 2 ranges only touch."
  [^CursorRange a ^CursorRange b]
  (let [a-start (cursor-range-start a)
        a-end (cursor-range-end a)
        b-start (cursor-range-start b)
        b-end (cursor-range-end b)]
    (cond
      ;; no intersection
      (or (cursor-before-or-same? b-end a-start)
          (cursor-before-or-same? a-end b-start))
      nil

      ;; A within B
      (and (cursor-before-or-same? b-start a-start)
           (cursor-before-or-same? a-end b-end))
      (when-not (cursor-range-empty? a) a)

      ;; B within A
      (and (cursor-before? a-start b-start)
           (cursor-before? b-end a-end))
      (when-not (cursor-range-empty? b) b)

      ;; intersection, B before A
      (cursor-before? b-end a-end)
      (->CursorRange a-start b-end)

      ;; intersection, A before B
      :else
      (->CursorRange b-start a-end))))

(defn- cursor-range-midpoint-follows? [^CursorRange cursor-range ^Cursor cursor]
  (let [start (cursor-range-start cursor-range)
        end (cursor-range-end cursor-range)
        mid-row (Math/floor (/ (+ (.row start) (.row end)) 2.0))]
    (cond (< (.row cursor) mid-row)
          true

          (> (.row cursor) mid-row)
          false

          (= (.row start) (.row end) (.row cursor))
          (< (.col cursor) (Math/floor (/ (+ (.col start) (.col end)) 2.0)))

          :else
          false)))

(defn- one-based->zero-based [index]
  (when (and (integer? index)
             (pos? ^long index))
    (dec ^long index)))

(defn line-number->CursorRange
  "Converts a one-based line number to a CursorRange. This is typically how
  external tools communicate line and column numbers to users. Will return nil
  unless given a positive integer. A one-based column or column range can
  optionally be specified in the same fashion. If not, column zero is used."
  (^CursorRange [line-number]
   (line-number->CursorRange line-number nil nil))
  (^CursorRange [line-number column]
   (line-number->CursorRange line-number column nil))
  (^CursorRange [line-number start-column end-column]
   (when-some [row (one-based->zero-based line-number)]
     (let [start-col (or (one-based->zero-based start-column) 0)
           end-col (or (one-based->zero-based end-column) start-col)]
       (->CursorRange (->Cursor row start-col)
                      (->Cursor row end-col))))))

(defn CursorRange->line-number
  "Returns the one-based line number from a CursorRange. This is typically how
  external tools communicate line numbers to users."
  ^long [^CursorRange cursor-range]
  (inc (.row (cursor-range-start cursor-range))))

(defn lines-reader
  ^Reader [lines]
  (let [*read-cursor (atom document-start-cursor)]
    (proxy [Reader] []
      (close [] (reset! *read-cursor nil))
      (read
        ([]
         (let [dest-buffer (char-array 1)]
           (if (= -1 (.read ^Reader this dest-buffer 0 1))
             -1
             (int (aget dest-buffer 0)))))
        ([dest-buffer]
         (if (instance? CharBuffer dest-buffer)
           (let [length (.remaining ^CharBuffer dest-buffer)
                 char-buffer (char-array length)
                 num-read (.read ^Reader this char-buffer 0 length)]
             (when (pos? num-read)
               (.put ^CharBuffer dest-buffer char-buffer 0 num-read))
             num-read)
           (.read ^Reader this dest-buffer 0 (alength ^chars dest-buffer))))
        ([dest-buffer dest-offset length]
         (let [*num-read (volatile! -1)]
           ;; Retry is safe, since the same range will be overwritten with the same source data.
           (swap! *read-cursor
                  (fn read [^Cursor read-cursor]
                    (if (nil? read-cursor)
                      (throw (IOException. "Cannot read from a closed lines-reader."))
                      (loop [^Cursor cursor read-cursor
                             ^long remaining length
                             ^long dest-offset dest-offset]
                        (let [row (.row cursor)
                              start-col (.col cursor)
                              ^String line (get lines row)]
                          (cond
                            ;; At end of stream?
                            (nil? line)
                            (let [num-read (- ^long length remaining)]
                              (vreset! *num-read (if (pos? num-read) num-read -1))
                              cursor)

                            ;; No more room in dest buffer?
                            (zero? remaining)
                            (let [num-read (- ^long length remaining)]
                              (vreset! *num-read num-read)
                              cursor)

                            ;; At end of last line?
                            (and (= row (dec (count lines))) (= start-col (count line)))
                            (recur (->Cursor (inc row) 0)
                                   remaining
                                   dest-offset)

                            ;; At end of other line?
                            (= start-col (count line))
                            (do (aset-char dest-buffer dest-offset \newline)
                                (recur (->Cursor (inc row) 0)
                                       (dec remaining)
                                       (inc dest-offset)))

                            :else
                            (let [line-remaining (- (count line) start-col)
                                  num-copied (min remaining line-remaining)
                                  end-col (+ start-col num-copied)]
                              (.getChars line start-col end-col dest-buffer dest-offset)
                              (recur (->Cursor row end-col)
                                     (- remaining num-copied)
                                     (+ dest-offset num-copied)))))))))
           @*num-read))))))

(defn lines-input-stream ^InputStream [lines]
  (ReaderInputStream. (lines-reader lines)))

(defn lines->string
  ^String [lines]
  (util/join-lines "\n" lines))

(defn string->lines [^String string]
  (util/split-lines string))

(defrecord Rect [^double x ^double y ^double w ^double h])

(defn rect-contains? [^Rect r ^double x ^double y]
  (and (<= (.x r) x (+ (.x r) (.w r)))
       (<= (.y r) y (+ (.y r) (.h r)))))

(defn expand-rect
  ^Rect [^Rect r ^double x ^double y]
  (->Rect (- (.x r) x)
          (- (.y r) y)
          (+ (.w r) (* 2.0 x))
          (+ (.h r) (* 2.0 y))))

(defn- outer-bounds
  ^Rect [^Rect a ^Rect b]
  (let [left   (min (.x a) (.x b))
        top    (min (.y a) (.y b))
        right  (max (+ (.x a) (.w a))
                    (+ (.x b) (.w b)))
        bottom (max (+ (.y a) (.h a))
                    (+ (.y b) (.h b)))
        width  (- right left)
        height (- bottom top)]
    (->Rect left top width height)))

(defrecord GestureInfo [type button ^long click-count ^double x ^double y])

(defn- button? [value]
  (case value (:primary :secondary :middle) true false))

(defn- keyword-map? [value]
  (and (map? value)
       (every? keyword? (keys value))))

(defn- gesture-info
  ^GestureInfo [type button click-count x y & {:as extras}]
  (assert (keyword? type))
  (assert (or (nil? button) (button? button)))
  (assert (and (integer? click-count) (pos? ^long click-count)))
  (assert (number? x))
  (assert (number? y))
  (assert (or (nil? extras) (keyword-map? extras)))
  (if (nil? extras)
    (->GestureInfo type button click-count x y)
    (merge (->GestureInfo type button click-count x y) extras)))

(defrecord LayoutInfo [line-numbers
                       canvas
                       minimap
                       glyph
                       tab-stops
                       scroll-tab-x
                       scroll-tab-y
                       ^double scroll-x
                       ^double scroll-y
                       ^double scroll-y-remainder
                       ^double document-width
                       ^long drawn-line-count
                       ^long dropped-line-count])

(defn- next-tab-stop-x
  ^double [tab-stops ^double x]
  (let [^double tab-width (:tab-width (first tab-stops))]
    (* (+ (quot x tab-width) 1) tab-width)))

(defn- advance-text-impl [glyph-metrics tab-stops ^String text start-index end-index start-x]
  (loop [^long index start-index
         ^double x start-x]
    (if (= ^long end-index index)
      x
      (let [glyph (.charAt text index)
            next-x (case glyph
                     \tab (next-tab-stop-x tab-stops x)
                     (+ x ^double (char-width glyph-metrics glyph)))]
        (recur (inc index) next-x)))))

(defn advance-text
  "Calculates the x position where text would end if drawn at the specified start x position in the document.
  This is not as simple as one would think, as tab stops and full-width characters must be taken into account."
  [^LayoutInfo layout ^String text start-index end-index start-x]
  (advance-text-impl (.glyph layout) (.tab-stops layout) text start-index end-index start-x))

(defn line-width
  "Returns an accurate line width measurement, taking tab stops into account."
  ^double [glyph-metrics tab-stops line]
  (advance-text-impl glyph-metrics tab-stops line 0 (count line) 0.0))

(defn text-width
  "Simple text width measurement. Does not take tab stops into account, so don't feed it strings with tabs.
  Instead, you should be using the advance-text function to find the x position where text ends."
  ^double [glyph-metrics ^String text]
  (reduce (fn ^double [^double text-width glyph]
            (if (Character/isISOControl ^char glyph)
              (throw (ex-info "Text must not contain control characters." {:text text :char-code (Character/hashCode glyph)}))
              (+ text-width ^double (char-width glyph-metrics glyph))))
          0.0
          text))

(def ^:private scroll-bar-size 14.0)
(def ^:private min-scroll-tab-size 30.0)

(defn- scroll-tab-size
  ^double [^double document-size ^double canvas-size]
  (if (< document-size ^double min-scroll-tab-size)
    min-scroll-tab-size
    (let [visible-ratio (min 1.0 (/ canvas-size document-size))]
      (max ^double min-scroll-tab-size (* canvas-size visible-ratio)))))

(defn- scroll-per-pixel
  ^double [^double document-size ^double canvas-size]
  (let [scroll-tab-size (scroll-tab-size document-size canvas-size)
        document-range (- document-size canvas-size)
        scroll-range (- canvas-size scroll-tab-size)]
    ;; FIXME can throw divide by zero if canvas height is the same as scroll tab height
    (/ document-range scroll-range)))

(defn max-line-width
  ([glyph-metrics tab-stops lines]
   (max-line-width glyph-metrics tab-stops lines 0 (count lines)))
  ([glyph-metrics tab-stops lines start-row end-row]
   (let [end-row (min ^long end-row (count lines))]
     (loop [^long row start-row
            max-width 0.0]
       (if (>= row end-row)
         max-width
         (recur (inc row)
                (max max-width (line-width glyph-metrics tab-stops (lines row)))))))))

(defn- scroll-width-ratio
  ^double [^LayoutInfo layout]
  (let [document-width (.document-width layout)
        canvas-width (.w ^Rect (.canvas layout))]
    (scroll-per-pixel document-width canvas-width)))

(defn- scroll-height-ratio
  ^double [^LayoutInfo layout ^long line-count]
  (let [^double line-height (line-height (.glyph layout))
        document-height (* line-count line-height)
        canvas-height (.h ^Rect (.canvas layout))]
    (scroll-per-pixel document-height canvas-height)))

(defn- scroll-tab-x-rect
  ^Rect [^Rect canvas-rect ^double document-width ^double scroll-x]
  (let [canvas-width (.w canvas-rect)]
    (when (< canvas-width document-width)
      (let [visible-left (- scroll-x)
            ^double scroll-bar-height scroll-bar-size
            scroll-bar-top (- (+ (.y canvas-rect) (.h canvas-rect)) scroll-bar-height)
            scroll-tab-width (scroll-tab-size document-width canvas-width)
            scroll-per-pixel (scroll-per-pixel document-width canvas-width)
            scroll-tab-left (+ (.x canvas-rect) (/ visible-left scroll-per-pixel))]
        (->Rect scroll-tab-left scroll-bar-top scroll-tab-width scroll-bar-height)))))

(defn- scroll-tab-y-rect
  ^Rect [^Rect canvas-rect line-height source-line-count dropped-line-count scroll-y-remainder]
  (let [document-height (* ^double source-line-count ^double line-height)
        canvas-height (.h canvas-rect)]
    (when (< canvas-height document-height)
      (let [visible-top (- (* ^double dropped-line-count ^double line-height) ^double scroll-y-remainder)
            ^double scroll-bar-width scroll-bar-size
            scroll-bar-left (- (+ (.x canvas-rect) (.w canvas-rect)) scroll-bar-width)
            scroll-tab-height (scroll-tab-size document-height canvas-height)
            scroll-per-pixel (scroll-per-pixel document-height canvas-height)
            scroll-tab-top (+ (.y canvas-rect) (/ visible-top scroll-per-pixel))]
        (->Rect scroll-bar-left scroll-tab-top scroll-bar-width scroll-tab-height)))))

(defn tab-stops [glyph-metrics tab-spaces]
  (let [^double space-width (char-width glyph-metrics \space)
        tab-width (max 1 (* space-width (double tab-spaces)))]
    ;; tab-stops is now a list of rules applicable for a range.
    ;; For now, there is one rule covering the whole x range.
    [{:tab-width tab-width}]))

(defn gutter-metrics [glyph-metrics ^double gutter-margin ^long source-line-count]
  (let [max-line-number-width (Math/ceil (* ^double (char-width glyph-metrics \0) (count (str source-line-count))))]
    [(+ gutter-margin max-line-number-width gutter-margin) gutter-margin]))

(defn- limit-scroll
  ^double [^double scroll-limit ^double scroll]
  (min 0.0 (max scroll scroll-limit)))

(defn layout-info
  ^LayoutInfo [canvas-width canvas-height document-width scroll-x scroll-y lines gutter-width gutter-margin glyph-metrics tab-spaces visible-minimap?]
  (let [^double line-height (line-height glyph-metrics)
        excluding-gutter-width (- ^double canvas-width ^double gutter-width)
        minimap-width (if visible-minimap? (min (Math/ceil (/ excluding-gutter-width 9.0)) 150.0) 0.0)
        canvas-rect-width (- excluding-gutter-width minimap-width)
        line-count (count lines)
        canvas-rect (->Rect ^double gutter-width 0.0 canvas-rect-width canvas-height)
        scroll-x (limit-scroll (- canvas-rect-width ^double document-width) scroll-x)
        ^Rect scroll-tab-x-rect (scroll-tab-x-rect canvas-rect document-width scroll-x)
        scroll-tab-x-height (double (if scroll-tab-x-rect (.h scroll-tab-x-rect) 0.0))
        scroll-y (limit-scroll (- ^double canvas-height (* line-height line-count) scroll-tab-x-height) scroll-y)
        dropped-line-count (long (/ scroll-y (- line-height)))
        scroll-y-remainder (double (mod scroll-y (- line-height)))
        drawn-line-count (long (Math/ceil (/ ^double (- ^double canvas-height scroll-y-remainder) line-height)))
        line-numbers-rect (->Rect ^double gutter-margin 0.0 (- ^double gutter-width (* 2.0 ^double gutter-margin)) canvas-height)
        minimap-left (- ^double canvas-width minimap-width)
        minimap-rect (->Rect minimap-left 0.0 minimap-width canvas-height)
        tab-stops (tab-stops glyph-metrics tab-spaces)
        scroll-tab-y-rect (scroll-tab-y-rect minimap-rect line-height line-count dropped-line-count (+ scroll-y-remainder scroll-tab-x-height))]
    (->LayoutInfo line-numbers-rect
                  canvas-rect
                  minimap-rect
                  glyph-metrics
                  tab-stops
                  scroll-tab-x-rect
                  scroll-tab-y-rect
                  scroll-x
                  scroll-y
                  scroll-y-remainder
                  document-width
                  drawn-line-count
                  dropped-line-count)))

(defn minimap-layout-info
  ^LayoutInfo [^LayoutInfo layout source-line-count glyph-metrics tab-spaces]
  (let [^Rect org-rect (.canvas layout)
        visible-height (.h org-rect)
        ^double document-line-height (line-height (.glyph layout))
        ^double minimap-line-height (line-height glyph-metrics)
        document-height (* ^double source-line-count document-line-height)
        document-scroll-y-range (- document-height visible-height)
        minimap-visible-height (* document-line-height (/ visible-height minimap-line-height))
        minimap-scroll-y-range (max 0.0 (- document-height minimap-visible-height))
        scroll-x 0.0
        scroll-y (if (zero? document-scroll-y-range)
                   0.0
                   (Math/rint (* (/ (.scroll-y layout) document-scroll-y-range)
                                 (/ minimap-line-height document-line-height)
                                 minimap-scroll-y-range)))
        dropped-line-count (long (/ scroll-y (- minimap-line-height)))
        scroll-y-remainder (double (mod scroll-y (- minimap-line-height)))
        drawn-line-count (long (Math/ceil (/ ^double (- visible-height scroll-y-remainder) minimap-line-height)))
        canvas-rect (.minimap layout)
        tab-stops (tab-stops glyph-metrics tab-spaces)]
    (assoc layout
      :canvas canvas-rect
      :glyph glyph-metrics
      :tab-stops tab-stops
      :scroll-x scroll-x
      :scroll-y scroll-y
      :scroll-y-remainder scroll-y-remainder
      :drawn-line-count drawn-line-count
      :dropped-line-count dropped-line-count)))

(defn- x->doc-x
  ^double [^LayoutInfo layout ^double x]
  (- x (.x ^Rect (.canvas layout)) (.scroll-x layout)))

(defn- y->doc-y
  ^double [^LayoutInfo layout ^double y]
  (- y (.y ^Rect (.canvas layout)) (.scroll-y layout)))

(defn- doc-x->x
  ^double [^LayoutInfo layout ^double doc-x]
  (+ doc-x (.x ^Rect (.canvas layout)) (.scroll-x layout)))

(defn- doc-y->y
  ^double [^LayoutInfo layout ^double doc-y]
  (+ doc-y (.y ^Rect (.canvas layout)) (.scroll-y layout)))

(defn row->y
  ^double [^LayoutInfo layout ^long row]
  (+ (.scroll-y-remainder layout)
     (* ^double (line-height (.glyph layout))
        (- row (.dropped-line-count layout)))))

(defn y->row
  ^long [^LayoutInfo layout ^double y]
  (+ (.dropped-line-count layout)
     (long (/ (- y (.scroll-y-remainder layout))
              ^double (line-height (.glyph layout))))))

(defn col->x
  ^double [^LayoutInfo layout ^long col ^String line]
  (+ (.x ^Rect (.canvas layout))
     (.scroll-x layout)
     ^double (advance-text layout line 0 col 0.0)))

(defn x->col
  ^long [^LayoutInfo layout ^double x ^String line]
  (let [line-x (x->doc-x layout x)
        line-length (count line)]
    (loop [col 0
           start-x 0.0]
      (if (<= line-length col)
        col
        (let [next-col (inc col)
              end-x (double (advance-text layout line col next-col start-x))]
          (if (<= end-x line-x)
            (recur next-col end-x)
            (max 0 (+ col (long (+ 0.5 (/ (- line-x start-x) (- end-x start-x))))))))))))

(defn x->character-col [^LayoutInfo layout ^double x ^String line]
  (let [line-x (x->doc-x layout x)
        line-length (count line)]
    (loop [col 0
           start-x 0.0]
      (if (<= line-length col)
        nil
        (let [next-col (inc col)
              end-x (double (advance-text layout line col next-col start-x))]
          (if (<= end-x line-x)
            (recur next-col end-x)
            (max 0 (+ col (long (/ (- line-x start-x) (- end-x start-x)))))))))))

(defn adjust-row
  ^long [lines ^long row]
  (max 0 (min row (dec (count lines)))))

(defn adjust-col
  ^long [lines ^long adjusted-row ^long col]
  (max 0 (min col (count (lines adjusted-row)))))

(defn adjust-cursor
  ^Cursor [lines ^Cursor cursor]
  (let [adjusted-row (adjust-row lines (.row cursor))
        adjusted-col (cond
                       (neg? (.row cursor))
                       0

                       (>= (.row cursor) (count lines))
                       (count (lines (dec (count lines))))

                       :else
                       (adjust-col lines adjusted-row (.col cursor)))]
    (assoc cursor :row adjusted-row :col adjusted-col)))

(defn adjust-cursor-range
  ^CursorRange [lines ^CursorRange cursor-range]
  (assoc cursor-range
    :from (adjust-cursor lines (.from cursor-range))
    :to (adjust-cursor lines (.to cursor-range))))

(defn cursor-ranges->rows [lines cursor-ranges]
  (into (sorted-set)
        (comp (map (partial adjust-cursor-range lines))
              (mapcat (fn [^CursorRange cursor-range]
                        (range (.row (cursor-range-start cursor-range))
                               (inc (.row (cursor-range-end cursor-range)))))))
        cursor-ranges))

(defn cursor-ranges->start-rows [lines cursor-ranges]
  (into (sorted-set)
        (map #(.row (cursor-range-start (adjust-cursor-range lines %))))
        cursor-ranges))

(defn- cursor-ranges->row-runs-xform [lines]
  (comp (map (partial adjust-cursor-range lines))
        (map (fn [cursor-range]
               (let [start-row (.row (cursor-range-start cursor-range))
                     end-row (inc (.row (cursor-range-end cursor-range)))]
                 (pair start-row end-row))))))

(defn cursor-ranges->row-runs [lines cursor-ranges]
  (into [] (cursor-ranges->row-runs-xform lines) cursor-ranges))

(defn canvas->cursor
  "Returns cursor closest to the mouse position

  See also: [[canvas->character-cursor]]"
  ^Cursor [^LayoutInfo layout lines x y]
  (let [row (y->row layout y)
        line (get lines (adjust-row lines row))
        col (x->col layout x line)]
    (->Cursor row col)))

(defn canvas->character-cursor
  "Returns cursor situated before the character that is hovered by the mouse

  See also: [[canvas->cursor]]"
  ^Cursor [^LayoutInfo layout lines x y]
  (let [row (y->row layout y)
        adjusted-row (adjust-row lines row)]
    (when (= adjusted-row row)
      (when-let [col (x->character-col layout x (get lines adjusted-row))]
        (->Cursor row col)))))

(defn- peek!
  "Like peek, but works for transient vectors."
  [coll]
  (get coll (dec (count coll))))

(defrecord Subsequence [^String first-line middle-lines ^String last-line])

(defn- empty-subsequence? [subsequence]
  (and (empty? (:first-line subsequence))
       (empty? (:middle-lines subsequence))
       (empty? (:last-line subsequence))))

(defn lines->subsequence
  ^Subsequence [lines]
  (let [line-count (count lines)
        first-line (first lines)
        middle-lines (when (< 2 line-count) (subvec lines 1 (dec line-count)))
        last-line (when (< 1 line-count) (peek lines))]
    (->Subsequence first-line middle-lines last-line)))

(defn subsequence->lines
  [subsequence]
  (let [last-line (:last-line subsequence)]
    (cond-> (into [(:first-line subsequence)]
                  (:middle-lines subsequence))
            (some? last-line) (conj last-line))))

(defn cursor-range-subsequence
  ^Subsequence [lines cursor-range]
  (let [start (adjust-cursor lines (cursor-range-start cursor-range))
        end (adjust-cursor lines (cursor-range-end cursor-range))]
    (if (= (.row start) (.row end))
      (->Subsequence (subs (lines (.row start)) (.col start) (.col end)) nil nil)
      (let [line-count (inc (- (.row end) (.row start)))
            start-line (lines (.row start))
            end-line (lines (.row end))]
        (->Subsequence (subs start-line (.col start) (count start-line))
                       (when (< 2 line-count) (subvec lines (inc (.row start)) (.row end)))
                       (when (< 1 line-count) (subs end-line 0 (.col end))))))))

(defn cursor-range-text
  ^String [lines cursor-range]
  (let [subsequence (cursor-range-subsequence lines cursor-range)]
    (if (and (empty? (:middle-lines subsequence))
             (empty? (:last-line subsequence)))
      (:first-line subsequence)
      (string/join "\n" (subsequence->lines subsequence)))))

(defn- append-subsequence! [runs subsequence]
  (let [runs' (if-some [joined-with-prev-line (not-empty (:first-line subsequence))]
                (let [prev-run (peek! runs)
                      prev-line (peek prev-run)]
                  (assert (= 1 (count prev-run)))
                  (assert (string? prev-line))
                  (conj! (pop! runs) (vector (str prev-line joined-with-prev-line))))
                runs)
        runs'' (if-some [middle-lines (not-empty (:middle-lines subsequence))]
                 (conj! runs' middle-lines)
                 runs')
        runs''' (if-some [last-line (:last-line subsequence)]
                  (conj! runs'' (vector last-line))
                  runs'')]
    runs'''))

(defn- prev-occurrence-search-cursor
  ^Cursor [cursor-ranges]
  (::prev-occurrence-search-cursor (meta cursor-ranges)))

(defn- with-prev-occurrence-search-cursor [cursor-ranges cursor]
  (assert (instance? Cursor cursor))
  (with-meta cursor-ranges {::prev-occurrence-search-cursor cursor}))

(defn- next-occurrence-search-cursor
  ^Cursor [cursor-ranges]
  (::next-occurrence-search-cursor (meta cursor-ranges)))

(defn- with-next-occurrence-search-cursor [cursor-ranges cursor]
  (assert (instance? Cursor cursor))
  (with-meta cursor-ranges {::next-occurrence-search-cursor cursor}))

(defn- col-to-col-pattern
  ^Pattern [needle-line case-sensitive? whole-word?]
  (re-pattern (str (when-not case-sensitive? "(?i)")
                   (when whole-word? "\\b")
                   (Pattern/quote needle-line)
                   (when whole-word? "\\b"))))

(defn- col-to-edge-pattern
  ^Pattern [needle-line case-sensitive? whole-word?]
  (re-pattern (str (when-not case-sensitive? "(?i)")
                   (when whole-word? "\\b")
                   (Pattern/quote needle-line)
                   "$")))

(defn- edge-to-col-pattern
  ^Pattern [needle-line case-sensitive? whole-word?]
  (re-pattern (str (when-not case-sensitive? "(?i)")
                   "^"
                   (Pattern/quote needle-line)
                   (when whole-word? "\\b"))))

(defn- edge-to-edge-pattern
  ^Pattern [needle-line case-sensitive?]
  (re-pattern (str (when-not case-sensitive? "(?i)")
                   "^"
                   (Pattern/quote needle-line)
                   "$")))

(defn- needle-patterns [needle-lines case-sensitive? whole-word?]
  (case (count needle-lines)
    1 [(col-to-col-pattern (needle-lines 0) case-sensitive? whole-word?)]
    2 [(col-to-edge-pattern (needle-lines 0) case-sensitive? whole-word?)
       (edge-to-col-pattern (needle-lines 1) case-sensitive? whole-word?)]
    [(col-to-edge-pattern (needle-lines 0) case-sensitive? whole-word?)
     (edge-to-edge-pattern (needle-lines 1) case-sensitive?)
     (edge-to-col-pattern (needle-lines 2) case-sensitive? whole-word?)]))

(defn find-prev-occurrence
  ^CursorRange [haystack-lines needle-lines ^Cursor adjusted-from-cursor case-sensitive? whole-word?]
  (let [needle-patterns (needle-patterns needle-lines case-sensitive? whole-word?)]
    (loop [cursor adjusted-from-cursor
           needles needle-patterns
           ^CursorRange matched-range nil]
      (if-some [needle (peek needles)]
        (when-some [haystack (get haystack-lines (.row cursor))]
          (let [matched-range' (some (fn [^MatchResult match-result]
                                       (when (<= (.end match-result) (.col cursor))
                                         (let [matched-from (->Cursor (.row cursor) (.start match-result))
                                               matched-to (if (some? matched-range)
                                                            (.to matched-range)
                                                            (->Cursor (.row cursor) (.end match-result)))]
                                           (->CursorRange matched-from matched-to))))
                                     (into '() (util/re-match-result-seq needle haystack)))
                needles' (if (some? matched-range')
                           (pop needles)
                           needle-patterns)
                cursor' (if (or (some? matched-range') (nil? matched-range))
                          (let [row (dec (.row cursor))
                                col (count (get haystack-lines row))]
                            (->Cursor row col))
                          cursor)]
            (recur cursor' needles' matched-range')))
        matched-range))))

(defn find-next-occurrence
  ^CursorRange [haystack-lines needle-lines ^Cursor adjusted-from-cursor case-sensitive? whole-word?]
  (let [needle-patterns (needle-patterns needle-lines case-sensitive? whole-word?)]
    (loop [cursor adjusted-from-cursor
           needles needle-patterns
           ^CursorRange matched-range nil]
      (if-some [needle (first needles)]
        (when-some [haystack (get haystack-lines (.row cursor))]
          (let [matcher (util/re-matcher-from needle haystack (.col cursor))
                matched-range' (when (.find matcher)
                                 (let [matched-from (if (some? matched-range)
                                                      (.from matched-range)
                                                      (->Cursor (.row cursor) (.start matcher)))
                                       matched-to (->Cursor (.row cursor) (.end matcher))]
                                   (->CursorRange matched-from matched-to)))
                needles' (if (some? matched-range')
                           (next needles)
                           needle-patterns)
                cursor' (if (or (some? matched-range') (nil? matched-range))
                          (->Cursor (inc (.row cursor)) 0)
                          cursor)]
            (recur cursor' needles' matched-range')))
        matched-range))))

(defn find-sequential-words-in-scope [haystack-lines needle-words ^CursorRange scope]
  (let [scope-end (cursor-range-end scope)]
    (loop [from-cursor (cursor-range-start scope)
           words needle-words
           matching-cursor-ranges (transient [])]
      (if-some [word (first words)]
        (if-some [matching-cursor-range (find-next-occurrence haystack-lines [word] from-cursor true false)]
          (if (neg? (compare-cursor-position scope-end (cursor-range-end matching-cursor-range)))
            (persistent! matching-cursor-ranges)
            (recur (cursor-range-end matching-cursor-range)
                   (next words)
                   (conj! matching-cursor-ranges matching-cursor-range)))
          (recur from-cursor
                 (next words)
                 matching-cursor-ranges))
        (persistent! matching-cursor-ranges)))))

(defn- word-cursor-range-at-cursor
  ^CursorRange [lines ^Cursor adjusted-cursor]
  (let [row (.row adjusted-cursor)
        line (lines row)
        line-length (count line)]
    (if (zero? line-length)
      (Cursor->CursorRange (->Cursor row 0))
      (let [on-whitespace? (and (whitespace-character-at-index? line (.col adjusted-cursor))
                                (or (zero? (.col adjusted-cursor))
                                    (whitespace-character-at-index? line (dec (.col adjusted-cursor)))))
            same-word? (if on-whitespace?
                         (partial whitespace-character-at-index? line)
                         (partial identifier-character-at-index? line))
            col (if (or (= line-length (.col adjusted-cursor))
                        (not (same-word? (.col adjusted-cursor))))
                  (max 0 (dec (.col adjusted-cursor)))
                  (.col adjusted-cursor))
            start-col (inc (or (long (first (drop-while same-word? (iterate dec col)))) -1))
            end-col (or (first (drop-while same-word? (iterate inc col))) line-length)]
        (->CursorRange (->Cursor row start-col)
                       (->Cursor row end-col))))))

(defn word-cursor-range? [lines ^CursorRange adjusted-cursor-range]
  (let [cursor (cursor-range-start adjusted-cursor-range)
        word-cursor-range (word-cursor-range-at-cursor lines cursor)]
    (and (not (cursor-range-empty? word-cursor-range))
         (cursor-range-equals? word-cursor-range adjusted-cursor-range)
         (not (Character/isWhitespace (.charAt ^String (lines (.row cursor)) (.col cursor)))))))

(defn selected-word-cursor-range
  ^CursorRange [lines cursor-ranges]
  (when-some [cursor-range (first cursor-ranges)]
    (let [word-cursor-range (adjust-cursor-range lines cursor-range)]
      (when (word-cursor-range? lines word-cursor-range)
        (let [word-subsequence (cursor-range-subsequence lines word-cursor-range)]
          (when (every? #(and (not (cursor-range-multi-line? %))
                              (= word-subsequence (cursor-range-subsequence lines %)))
                        (take 64 cursor-ranges))
            word-cursor-range))))))

(defn suggestion-query-cursor-range
  ^CursorRange [lines cursor-ranges]
  (let [cursor (some->> cursor-ranges first CursorRange->Cursor (adjust-cursor lines))
        word-cursor-range (word-cursor-range-at-cursor lines cursor)
        start (cursor-range-start word-cursor-range)
        query-cursor-range (->CursorRange start cursor)
        line (lines (.row cursor))]
    (if (or (= \. (get line (dec (.col cursor))))
            (= \. (get line (dec (.col start))))
            (= \: (get line (dec (.col cursor))))
            (= \: (get line (dec (.col start)))))
      (let [cursor-before-dot (->Cursor (.row start) (dec (.col start)))
            word-cursor-range-before-dot (word-cursor-range-at-cursor lines cursor-before-dot)]
        (->CursorRange (cursor-range-start word-cursor-range-before-dot) cursor))
      query-cursor-range)))

(defn- cursor-framing-rect
  ^Rect [^LayoutInfo layout lines ^Cursor adjusted-cursor]
  (let [line (lines (.row adjusted-cursor))
        left (col->x layout (.col adjusted-cursor) line)
        top (row->y layout (.row adjusted-cursor))]
    (->Rect left top 1.0 (line-height (.glyph layout)))))

(defn cursor-rect
  ^Rect [^LayoutInfo layout lines ^Cursor adjusted-cursor]
  (let [line (lines (.row adjusted-cursor))
        left (- (col->x layout (.col adjusted-cursor) line) 0.5)
        top (- (row->y layout (.row adjusted-cursor)) 0.5)]
    (->Rect left top 1.0 (inc ^double (line-height (.glyph layout))))))

(defn cursor-range-rects
  [^LayoutInfo layout lines ^CursorRange adjusted-cursor-range]
  (let [canvas ^Rect (.canvas layout)
        ^double line-height (line-height (.glyph layout))
        col-to-col-rect (fn [^long row ^long start-col ^long end-col]
                          (let [line (lines row)
                                top (row->y layout row)
                                left (col->x layout start-col line)
                                right (col->x layout end-col line)
                                width (- right left)]
                            (->Rect left top width line-height)))
        col-to-edge-rect (fn [^long row ^long col]
                           (let [line (lines row)
                                 top (row->y layout row)
                                 left (col->x layout col line)
                                 width (- (+ (.x canvas) (.w canvas)) left)]
                             (->Rect left top width line-height)))
        edge-to-col-rect (fn [^long row ^long col]
                           (let [line (lines row)
                                 top (row->y layout row)
                                 left (col->x layout 0 line)
                                 right (col->x layout col line)
                                 width (- right left)]
                             (->Rect left top width line-height)))
        edge-to-edge-rect (fn [^long start-row ^long end-row]
                            (let [top (row->y layout start-row)
                                  left (col->x layout 0 "")
                                  width (- (+ (.x canvas) (.w canvas)) left)
                                  height (* line-height (inc (- end-row start-row)))]
                              (->Rect left top width height)))
        {start-row :row start-col :col} (cursor-range-start adjusted-cursor-range)
        {end-row :row end-col :col} (cursor-range-end adjusted-cursor-range)]
    (case (- ^long end-row ^long start-row)
      0 [(col-to-col-rect start-row start-col end-col)]
      1 [(col-to-edge-rect start-row start-col)
         (edge-to-col-rect end-row end-col)]
      [(col-to-edge-rect start-row start-col)
       (edge-to-edge-rect (inc ^long start-row) (dec ^long end-row))
       (edge-to-col-rect end-row end-col)])))

(defn- scroll-x-limit
  ^double [^LayoutInfo layout]
  (let [canvas-width (.w ^Rect (.canvas layout))
        document-width (.document-width layout)]
    (- canvas-width document-width)))

(defn- scroll-y-limit
  ^double [^LayoutInfo layout ^long line-count]
  (let [^double line-height (line-height (.glyph layout))
        document-height (* line-count line-height)
        canvas-height (.h ^Rect (.canvas layout))
        ^Rect scroll-tab-x (.scroll-tab-x layout)
        scroll-tab-x-h (double (if scroll-tab-x (.h scroll-tab-x) 0.0))]
    (- canvas-height document-height scroll-tab-x-h)))

(defn limit-scroll-x
  ^double [^LayoutInfo layout ^double scroll-x]
  (limit-scroll (scroll-x-limit layout) scroll-x))

(defn limit-scroll-y
  ^double [^LayoutInfo layout ^long line-count ^double scroll-y]
  (limit-scroll (scroll-y-limit layout line-count) scroll-y))

(defn scroll-to-bottom
  ^double [^LayoutInfo layout ^long line-count]
  (limit-scroll (scroll-y-limit layout line-count) Double/NEGATIVE_INFINITY))

(defn scrolled-to-bottom? [^LayoutInfo layout ^long line-count]
  (let [^double line-height (line-height (.glyph layout))
        document-height (* line-count line-height)
        canvas-height (.h ^Rect (.canvas layout))
        scroll-min (- canvas-height document-height)]
    (>= scroll-min (.scroll-y layout))))

(defn- scroll-center [_margin canvas-offset canvas-size target-offset target-size scroll-offset]
  (let [canvas-min (- ^double canvas-offset ^double scroll-offset)
        canvas-max (+ canvas-min ^double canvas-size)
        target-min (- ^double target-offset ^double scroll-offset)
        target-max (+ target-min ^double target-size)]
    (when (or (< target-min canvas-min) (> target-max canvas-max))
      (- ^double scroll-offset
         (- (/ ^double canvas-size 2.0))
         (+ ^double target-offset (/ ^double target-size 2.0))))))

(defn- scroll-shortest [margin canvas-offset canvas-size target-offset target-size scroll-offset]
  (let [canvas-min (- ^double canvas-offset ^double scroll-offset)
        canvas-max (+ canvas-min ^double canvas-size)
        target-min (- ^double target-offset ^double scroll-offset)
        target-max (+ target-min ^double target-size)]
    (when (or (< target-min canvas-min) (> target-max canvas-max))
      (let [scroll-min (+ ^double scroll-offset (- canvas-min target-min))
            scroll-max (+ ^double scroll-offset (- canvas-max target-max))]
        (if (< (Math/abs (- scroll-min ^double scroll-offset))
               (Math/abs (- scroll-max ^double scroll-offset)))
          (+ scroll-min ^double margin)
          (- scroll-max ^double margin))))))

(def scroll-nowhere (constantly nil))

(defn- scroll-to-rect [scroll-x-fn scroll-y-fn ^LayoutInfo layout lines ^Rect target-rect]
  (let [canvas-rect ^Rect (.canvas layout)
        margin-x (text-width (.glyph layout) "    ")
        margin-y 0.0
        scroll-x (or (scroll-x-fn margin-x (.x canvas-rect) (.w canvas-rect) (.x target-rect) (.w target-rect) (.scroll-x layout)) (.scroll-x layout))
        scroll-x (limit-scroll-x layout (Math/floor scroll-x))
        scroll-y (or (scroll-y-fn margin-y (.y canvas-rect) (.h canvas-rect) (.y target-rect) (.h target-rect) (.scroll-y layout)) (.scroll-y layout))
        scroll-y (limit-scroll-y layout (count lines) (Math/floor scroll-y))]
    (cond-> nil
            (not= (.scroll-x layout) scroll-x) (assoc :scroll-x scroll-x)
            (not= (.scroll-y layout) scroll-y) (assoc :scroll-y scroll-y))))

(defn- scroll-to-cursor [scroll-x-fn scroll-y-fn ^LayoutInfo layout lines ^Cursor adjusted-cursor]
  (let [target-rect (cursor-framing-rect layout lines adjusted-cursor)]
    (scroll-to-rect scroll-x-fn scroll-y-fn layout lines target-rect)))

(defn- scroll-to-cursor-range [scroll-x-fn scroll-y-fn ^LayoutInfo layout lines ^CursorRange adjusted-cursor-range]
  (let [target-rect (reduce outer-bounds (cursor-range-rects layout lines adjusted-cursor-range))]
    (scroll-to-rect scroll-x-fn scroll-y-fn layout lines target-rect)))

(defn- scroll-distance
  ^double [^double from to]
  (if (some? to)
    (Math/abs (- ^double to from))
    0.0))

(defn- compare-scroll-severity
  ^long [^LayoutInfo layout a b]
  (util/comparisons
    (compare (scroll-distance (.scroll-y layout) (:scroll-y a))
             (scroll-distance (.scroll-y layout) (:scroll-y b)))
    (compare (scroll-distance (.scroll-x layout) (:scroll-x a))
             (scroll-distance (.scroll-x layout) (:scroll-x b)))))

(defn scroll-to-any-cursor [^LayoutInfo layout lines cursor-ranges]
  (reduce (fn [shortest-scroll scroll]
            (cond (nil? scroll) (reduced nil) ;; Early-out: No scroll required.
                  (neg? (compare-scroll-severity layout scroll shortest-scroll)) scroll
                  :else shortest-scroll))
          (sequence (comp (map CursorRange->Cursor)
                          (map (partial adjust-cursor lines))
                          (map (partial scroll-to-cursor scroll-shortest scroll-shortest layout lines)))
                    cursor-ranges)))

(defn- update-layout-from-props
  ^LayoutInfo [^LayoutInfo layout {:keys [document-width] :as _props}]
  (assert (or (nil? document-width) (instance? Double document-width)))
  (if (some? document-width)
    (assoc layout :document-width document-width)
    layout))

(defn frame-cursor [{:keys [lines cursor-ranges] :as props} ^LayoutInfo layout]
  (when (some? props)
    (assert (vector? lines))
    (assert (vector? cursor-ranges))
    (merge props (scroll-to-any-cursor (update-layout-from-props layout props) lines cursor-ranges))))

(defn ensure-syntax-info
  "ensure syntax info is calculated up to end-row

  Important: end-row is 1-indexed. If you want syntax for the first line of the
  document, you need to provide 1 instead of 0!"
  [syntax-info ^long end-row lines grammar]
  (let [valid-count (count syntax-info)
        end-row (min (count lines) end-row)]
    (if (<= end-row valid-count)
      syntax-info
      (loop [syntax-info' (transient syntax-info)
             lines (subvec lines valid-count end-row)
             contexts (or (first (peek syntax-info))
                          (list (syntax/make-context (:scope-name grammar) (:patterns grammar))))]
        (if-some [line (first lines)]
          (let [[contexts :as entry] (syntax/analyze contexts line)]
            (recur (conj! syntax-info' entry)
                   (next lines)
                   contexts))
          (persistent! syntax-info'))))))

(defn syntax-scope-before-cursor [syntax-info grammar ^Cursor cursor]
  {:pre [(< (.-row cursor) (count syntax-info))]}
  (let [row (.-row cursor)
        col-before-cursor (dec (.-col cursor))
        runs (second (get syntax-info row))
        result-index (dec ^long (util/find-insert-index runs [col-before-cursor] #(compare (%1 0) (%2 0))))]
    (if-let [run (get runs result-index)]
      (second run)
      (:scope-name grammar "source"))))

(defn last-visible-row
  "Returns 1-indexed row number of the last visible row"
  [^LayoutInfo layout]
  (let [start-row (.dropped-line-count layout)]
    (+ start-row (.drawn-line-count layout))))

(defn invalidate-syntax-info [syntax-info ^long invalidated-row ^long line-count]
  (into [] (subvec syntax-info 0 (min invalidated-row line-count (count syntax-info)))))

(defn offset-cursor
  ^Cursor [^Cursor cursor ^long row-offset ^long col-offset]
  (if (and (zero? row-offset)
           (zero? col-offset))
    cursor
    (-> cursor
        (update :row (fn [^long row] (+ row row-offset)))
        (update :col (fn [^long col] (+ col col-offset))))))

(defn offset-cursor-range
  ^CursorRange [^CursorRange cursor-range offset-type ^long row-offset ^long col-offset]
  (if (and (zero? row-offset)
           (zero? col-offset))
    cursor-range
    (let [from (.from cursor-range)
          to (.to cursor-range)
          range-inverted? (pos? (compare-cursor-position from to))
          start (if range-inverted? to from)
          end (if range-inverted? from to)
          start' (case offset-type
                   (:start :both) (offset-cursor start row-offset col-offset)
                   start)
          end' (case offset-type
                 (:end :both) (offset-cursor end row-offset col-offset)
                 end)]
      (assoc cursor-range
        :from (if range-inverted? end' start')
        :to (if range-inverted? start' end')))))

(defn- cursor-offset-up
  ^Cursor [^long row-count lines ^Cursor cursor]
  (if (zero? row-count)
    cursor
    (let [row (adjust-row lines (.row cursor))
          col (.col cursor)
          new-row (adjust-row lines (- row row-count))
          new-col (if (zero? row) 0 col)]
      (->Cursor new-row new-col))))

(defn- cursor-offset-down
  ^Cursor [^long row-count lines ^Cursor cursor]
  (if (zero? row-count)
    cursor
    (let [row (adjust-row lines (.row cursor))
          col (.col cursor)
          last-row (dec (count lines))
          new-row (adjust-row lines (+ row row-count))
          new-col (if (= last-row row) (count (lines last-row)) col)]
      (->Cursor new-row new-col))))

(defn- text-start
  ^long [lines ^long row]
  (count (take-while #(Character/isWhitespace ^char %) (lines row))))

(defn- at-text-start? [lines ^Cursor adjusted-cursor]
  (= (.col adjusted-cursor) (text-start lines (.row adjusted-cursor))))

(defn- cursor-home
  ^Cursor [at-text-start? lines ^Cursor cursor]
  (let [row (adjust-row lines (.row cursor))
        col (if at-text-start? 0 (text-start lines row))]
    (->Cursor row col)))

(defn- cursor-line-start
  ^Cursor [lines ^Cursor cursor]
  (let [row (adjust-row lines (.row cursor))]
    (->Cursor row 0)))

(defn- cursor-line-end
  ^Cursor [lines ^Cursor cursor]
  (let [row (adjust-row lines (.row cursor))
        col (count (lines row))]
    (->Cursor row col)))

(defn- cursor-file-start
  ^Cursor [_lines ^Cursor _cursor]
  (->Cursor 0 0))

(defn- cursor-file-end
  ^Cursor [lines ^Cursor _cursor]
  (let [row (dec (count lines))
        col (count (lines row))]
    (->Cursor row col)))

(defn- cursor-up
  ^Cursor [lines ^Cursor cursor]
  (cursor-offset-up 1 lines cursor))

(defn- cursor-down
  ^Cursor [lines ^Cursor cursor]
  (cursor-offset-down 1 lines cursor))

(defn- cursor-left
  ^Cursor [lines ^Cursor cursor]
  (let [adjusted (adjust-cursor lines cursor)
        row (.row adjusted)
        col (.col adjusted)
        new-col (dec col)]
    (if (neg? new-col)
      (let [new-row (max 0 (dec row))
            new-col (if (zero? row) 0 (count (lines new-row)))]
        (->Cursor new-row new-col))
      (->Cursor row new-col))))

(defn- cursor-right
  ^Cursor [lines ^Cursor cursor]
  (let [adjusted (adjust-cursor lines cursor)
        row (.row adjusted)
        col (.col adjusted)
        new-col (inc col)]
    (if (> new-col (count (lines row)))
      (let [last-row (dec (count lines))
            new-row (min (inc row) last-row)
            new-col (if (= last-row row) (count (lines last-row)) 0)]
        (->Cursor new-row new-col))
      (->Cursor row new-col))))

(defn- cursor-prev-word
  ^Cursor [lines ^Cursor cursor]
  (let [left-adjusted (cursor-left lines cursor)]
    (if (not= (.row cursor) (.row left-adjusted))
      left-adjusted
      (cursor-range-start (word-cursor-range-at-cursor lines left-adjusted)))))

(defn- cursor-next-word
  ^Cursor [lines ^Cursor cursor]
  (let [right-adjusted (cursor-right lines cursor)]
    (if (not= (.row cursor) (.row right-adjusted))
      right-adjusted
      (cursor-range-end (word-cursor-range-at-cursor lines right-adjusted)))))

(defn move-cursors [cursor-ranges move-fn lines]
  (into []
        (comp (map CursorRange->Cursor)
              (map (partial move-fn lines))
              (distinct)
              (map Cursor->CursorRange))
        cursor-ranges))

(defn- try-merge-cursor-range-pair [^CursorRange a ^CursorRange b]
  (let [as (cursor-range-start a)
        ae (cursor-range-end a)
        bs (cursor-range-start b)
        be (cursor-range-end b)
        as->bs (compare-cursor-position as bs)
        ae->be (compare-cursor-position ae be)]

    ;;  =>  <=
    ;;  =>  <A   <B
    ;;  =>  <B   <A
    ;;  A>   B>  <=
    ;;  B>   A>  <=
    ;;  A>  <A    B>  <B
    ;;  A>   B>  <A   <B
    ;;  A>   B>  <B   <A
    ;;  B>  <B    A>  <A
    ;;  B>   A>  <B   <A
    ;;  B>   A>  <A   <B
    ;;  A>  <=>  <B
    ;;  B>  <=>  <A
    ;;  A>  <=>
    ;;  B>  <=>
    ;; <=>  <A
    ;; <=>  <B
    (cond
      (neg? as->bs)
      ;;  A>   B>  <=
      ;;  A>  <A    B>  <B
      ;;  A>   B>  <A   <B
      ;;  A>   B>  <B   <A
      ;;  A>  <=>  <B
      ;;  A>  <=>
      (cond
        (neg? ae->be)
        ;;  A>  <A    B>  <B
        ;;  A>   B>  <A   <B
        ;;  A>  <=>  <B
        (cond
          (pos? (compare-cursor-position ae bs))
          ;;  A>   B>  <A   <B
          (if (neg? (compare-cursor-position (.from b) (.to b)))
            (assoc b :from as :to be)
            (assoc b :from be :to as))

          :else
          ;;  A>  <A    B>  <B
          ;;  A>  <=>  <B
          nil)

        (zero? ae->be)
        ;;  A>   B>  <=
        a

        (pos? ae->be)
        ;; A>  B>  <B  <A
        a)

      (zero? as->bs)
      ;;  =>  <=
      ;;  =>  <A   <B
      ;;  =>  <B   <A
      ;; <=>  <A
      ;; <=>  <B
      (cond
        (neg? ae->be)
        ;;  =>  <A   <B
        ;; <=>  <B
        b

        (zero? ae->be)
        ;;  =>  <=
        a

        (pos? ae->be)
        ;;  =>  <B   <A
        ;; <=>  <A
        a)

      (pos? as->bs)
      ;;  B>   A>  <=
      ;;  B>  <B    A>  <A
      ;;  B>   A>  <B   <A
      ;;  B>   A>  <A   <B
      ;;  B>  <=>  <A
      ;;  B>  <=>
      (cond
        (neg? ae->be)
        ;;  B>   A>  <A   <B
        b

        (zero? ae->be)
        ;;  B>   A>   <=
        ;;  B>  <=>
        b

        (pos? ae->be)
        ;;  B>  <B    A>  <A
        ;;  B>   A>  <B   <A
        ;;  B>  <=>  <A
        (cond
          (pos? (compare-cursor-position be as))
          ;;  B>   A>  <B   <A
          (if (neg? (compare-cursor-position (.from b) (.to b)))
            (assoc b :from bs :to ae)
            (assoc b :from ae :to bs))

          :else
          ;;  B>  <B    A>  <A
          ;;  B>  <=>  <A
          nil)))))

(defn- merge-cursor-ranges [ascending-cursor-ranges]
  (reduce (fn [merged-cursor-ranges ^CursorRange cursor-range]
            (if-some [[index updated-cursor-range]
                      (first (keep-indexed (fn [index merged-cursor-range]
                                             (when (= (:type merged-cursor-range) (:type cursor-range))
                                               (when-some [updated-cursor-range (try-merge-cursor-range-pair merged-cursor-range cursor-range)]
                                                 [index updated-cursor-range])))
                                           merged-cursor-ranges))]
              (assoc merged-cursor-ranges index updated-cursor-range)
              (conj merged-cursor-ranges cursor-range)))
          []
          ascending-cursor-ranges))

(def ^:private concat-cursor-ranges (comp merge-cursor-ranges sort concat))

(defn extend-selection [ascending-cursor-ranges move-fn lines]
  (merge-cursor-ranges
    (map (fn [^CursorRange cursor-range]
           (assoc cursor-range :to (move-fn lines (.to cursor-range))))
         ascending-cursor-ranges)))

(defn- move-type->move-fn [move-type]
  (case move-type
    :navigation move-cursors
    :selection extend-selection))

(defn splice-lines [lines ascending-cursor-ranges-and-replacements]
  (let [new-lines (into []
                        cat
                        (loop [start (->Cursor 0 0)
                               rest ascending-cursor-ranges-and-replacements
                               lines-seqs (transient [[""]])]
                          (if-some [[cursor-range replacement-lines] (first rest)]
                            (let [prior-end (adjust-cursor lines (cursor-range-start cursor-range))]
                              (recur (cursor-range-end cursor-range)
                                     (next rest)
                                     (cond-> lines-seqs
                                             (neg? (compare-cursor-position start prior-end)) (append-subsequence! (cursor-range-subsequence lines (->CursorRange start prior-end)))
                                             (seq replacement-lines) (append-subsequence! (lines->subsequence replacement-lines)))))
                            (let [end (->Cursor (dec (count lines)) (count (peek lines)))
                                  end-seq (cursor-range-subsequence lines (->CursorRange start end))]
                              (persistent! (if (empty-subsequence? end-seq)
                                             lines-seqs
                                             (append-subsequence! lines-seqs end-seq)))))))
        previous-version (hash lines)
        change {:previous-version previous-version
                :current-version (hash new-lines)
                :ascending-cursor-ranges-and-replacements (mapv (fn [[cursor-range replacement]]
                                                                  [(adjust-cursor-range lines cursor-range) replacement])
                                                                ascending-cursor-ranges-and-replacements)}]
    (with-meta
      new-lines
      (update (meta lines)
              ::changes
              (fn [changes]
                (if (or (nil? changes)
                        (not= previous-version (:current-version (peek changes))))
                  [change]
                  (-> changes
                      (conj change)
                      ;; max incremental history size is 16
                      (cond->> (= 16 (count changes)) (into [] (drop 1))))))))))

(defn get-incremental-diff
  "If there is an incremental diff between old and new lines, return it

  Returns a vector of [cursor-range replacement-vec-of-strings] that, when
  applied one after another to old lines, will result in new lines, or nil if
  no such transformation exists"
  [old-lines new-lines]
  (let [changes (::changes (meta new-lines))]
    (when (and changes (= (hash new-lines) (:current-version (peek changes))))
      (let [old-version (hash old-lines)
            last-version-index (long (loop [i (dec (count changes))]
                                       (cond
                                         (neg? i) i
                                         (= old-version (:previous-version (changes i))) i
                                         :else (recur (dec i)))))]
        (when-not (neg? last-version-index)
          (let [ret (into
                      []
                      (comp
                        (map :ascending-cursor-ranges-and-replacements)
                        (mapcat
                          (fn [ascending-cursor-ranges-and-replacements]
                            (eduction
                              (map (fn [[cursor-range replacement]]
                                     [(->CursorRange
                                        (cursor-range-start cursor-range)
                                        (cursor-range-end cursor-range))
                                      replacement]))
                              (reverse ascending-cursor-ranges-and-replacements)))))
                      (subvec changes last-version-index))]
            (when (pos? (count ret))
              ret)))))))

(defn- offset-cursor-on-row
  ^Cursor [^Cursor cursor ^long col-affected-row ^long row-offset ^long col-offset]
  (let [row (.row cursor)
        col (.col cursor)
        row' (+ row row-offset)
        col' (if (= col-affected-row row)
               (+ col col-offset)
               col)]
    (if (and (= row row') (= col col'))
      cursor
      (assoc cursor :row row' :col col'))))

(defn- append-offset-cursor! [col-affected-row row-offset col-offset cursors ^Cursor cursor]
  (conj! cursors (offset-cursor-on-row cursor col-affected-row row-offset col-offset)))

(defn- unpack-cursor-range [^CursorRange cursor-range]
  (let [from (assoc (.from cursor-range) :field :from)
        to (assoc (.to cursor-range) :field :to)
        range-inverted? (pos? (compare-cursor-position from to))
        start (assoc (if range-inverted? to from) :order :start)
        end (assoc (if range-inverted? from to) :order :end)]
    (pair start end)))

(defn- pack-cursor-range
  ^CursorRange [^CursorRange cursor-range ^Cursor start ^Cursor end]
  (assert (or (nil? start) (= :start (:order start))))
  (assert (or (nil? end) (= :end (:order end))))
  (cond
    (or (nil? start) (nil? end))
    nil

    (and (= :from (:field start))
         (= :to (:field end)))
    (assoc cursor-range
      :from (dissoc start :field :order)
      :to (dissoc end :field :order))

    (and (= :from (:field end))
         (= :to (:field start)))
    (assoc cursor-range
      :from (dissoc end :field :order)
      :to (dissoc start :field :order))

    :else
    (throw (ex-info "Found cursor with invalid field info"
                    {:start start
                     :end end}))))

(defrecord SpliceInfo [^Cursor start ^Cursor end ^long row-offset ^long col-offset])

(defn- col-affected-row
  ^long [^SpliceInfo splice-info]
  (if (some? splice-info)
    (.row ^Cursor (.end splice-info))
    -1))

(defn- row-offset
  ^long [^SpliceInfo splice-info]
  (or (some-> splice-info .row-offset) 0))

(defn- col-offset
  ^long [^SpliceInfo splice-info]
  (or (some-> splice-info .col-offset) 0))

(defn- accumulate-splice
  ^SpliceInfo [^SpliceInfo prev-splice-info cursor-range-and-replacement]
  (when (some? cursor-range-and-replacement)
    (let [[^CursorRange snip replacement-lines] cursor-range-and-replacement
          start (cursor-range-start snip)
          end (cursor-range-end snip)
          last-line-length (count (peek replacement-lines))
          row-count-before (inc (- (.row end) (.row start)))
          row-count-after (count replacement-lines)
          row-difference (- row-count-after row-count-before)
          end-col-before (.col end)
          multi-line-replacement? (> row-count-after 1)
          ^long end-col-after (if multi-line-replacement? last-line-length (+ (.col start) last-line-length))
          col-difference (- end-col-after end-col-before)
          row-offset (row-offset prev-splice-info)
          col-offset (if (and (not multi-line-replacement?)
                              (= (.row start) (col-affected-row prev-splice-info)))
                       (col-offset prev-splice-info)
                       0)
          row-offset' (+ row-offset row-difference)
          col-offset' (+ col-offset col-difference)]
      (->SpliceInfo start end row-offset' col-offset'))))

(defn- splice-relative-cursor->absolute-cursor
  "Adjust splice-relative cursor given an absolute cursor the defines the
  beginning of the splice"
  ^Cursor [^Cursor splice-relative-cursor ^Cursor absolute-cursor]
  (let [row (.-row absolute-cursor)
        col (.-col absolute-cursor)
        same-line (zero? (.-row splice-relative-cursor))]
    (-> splice-relative-cursor
        (update :row + row)
        (cond-> same-line (update :col + col)))))

(defn- splice-relative-cursor-range->absolute-cursor-range
  "Adjust splice-relative cursor range given an absolute cursor that defines the
  beginning of the splice"
  ^CursorRange [^CursorRange splice-relative-cursor-range ^Cursor absolute-cursor]
  (-> splice-relative-cursor-range
      (update :from splice-relative-cursor->absolute-cursor absolute-cursor)
      (update :to splice-relative-cursor->absolute-cursor absolute-cursor)))

(defn- introduce-replacement-ranges [ascending-cursor-ranges-and-replacements]
  (loop [splice-info nil
         splices (seq ascending-cursor-ranges-and-replacements)
         acc (transient [])]
    (if-let [splice (first splices)]
      (let [[cursor-range _ splice-relative-regions] splice]
        (recur (accumulate-splice splice-info splice)
               (next splices)
               (if splice-relative-regions
                 (let [start-cursor (offset-cursor-on-row
                                      (cursor-range-start cursor-range)
                                      (col-affected-row splice-info)
                                      (row-offset splice-info)
                                      (col-offset splice-info))]
                   (transduce
                     (map #(splice-relative-cursor-range->absolute-cursor-range
                             % start-cursor))
                     conj!
                     acc
                     splice-relative-regions))
                 acc)))
      (persistent! acc))))

(defn- splice-cursors
  "Given cursors and ranges+replacements, return a vector of the same length
  with adjusted cursors. Result may contain nils, which means the cursor is
  deleted"
  [ascending-cursors ascending-cursor-ranges-and-replacements]
  (loop [prev-splice-info nil
         splice-info (accumulate-splice prev-splice-info (first ascending-cursor-ranges-and-replacements))
         rest-splices (next ascending-cursor-ranges-and-replacements)
         cursors ascending-cursors
         spliced-cursors (transient [])]
    (if (nil? splice-info)
      ;; We have no more splices. Apply the offset to the remaining cursors.
      (persistent! (reduce (partial append-offset-cursor!
                                    (col-affected-row prev-splice-info)
                                    (row-offset prev-splice-info)
                                    (col-offset prev-splice-info))
                           spliced-cursors
                           cursors))

      ;; We have a splice.
      (let [cursor (first cursors)]
        (if (nil? cursor)
          ;; We have no more cursors, so we're done!
          (persistent! spliced-cursors)

          ;; We have a cursor.
          (cond
            (let [start-comparison (compare-cursor-position cursor (.start splice-info))]
              (or (neg? start-comparison)
                  (and (zero? start-comparison)
                       (= :start (:order cursor))
                       (nil? (::sticky cursor)))))
            ;; The cursor is before the splice start.
            ;; Apply accumulated offset to the cursor and move on to the next cursor.
            (recur prev-splice-info
                   splice-info
                   rest-splices
                   (next cursors)
                   (conj! spliced-cursors
                          (offset-cursor-on-row cursor
                                                (col-affected-row prev-splice-info)
                                                (row-offset prev-splice-info)
                                                (col-offset prev-splice-info))))

            (neg? (compare-cursor-position cursor (.end splice-info)))
            ;; The cursor is inside the splice.
            ;; Append a nil cursor to flag the range for removal and move on to the next cursor,
            ;; or push cursor outside of range if it is sticky, e.g. has ::sticky set to either:
            ;; - :left (will move cursor to the left of splice)
            ;; - :right (will move cursor to the right of splice)
            (recur prev-splice-info
                   splice-info
                   rest-splices
                   (next cursors)
                   (conj! spliced-cursors
                          (case (::sticky cursor)
                            :left
                            (let [^Cursor left-edge (.start splice-info)]
                              (assoc cursor :col (.col left-edge) :row (.row left-edge)))

                            :right
                            (let [^Cursor right-edge (offset-cursor-on-row
                                                       (.end splice-info)
                                                       (col-affected-row splice-info)
                                                       (row-offset splice-info)
                                                       (col-offset splice-info))]
                              (assoc cursor :col (.col right-edge) :row (.row right-edge)))

                            nil)))

            :else
            ;; The cursor is after the splice end.
            ;; Accumulate offset from splice and move on to the next splice.
            (recur splice-info
                   (accumulate-splice splice-info (first rest-splices))
                   (next rest-splices)
                   cursors
                   spliced-cursors)))))))

(defn- splice-cursor-ranges [ascending-cursor-ranges ascending-cursor-ranges-and-replacements]
  (let [;; ranges -> cursors
        cursors (sequence (mapcat unpack-cursor-range) ascending-cursor-ranges)
        ;; [[i cursor] ...]
        indexed-cursors (map-indexed vector cursors)
        ;; [[i cursor] ...], but sorted by cursor
        ascending-indexed-cursors (sort-by second compare-cursor-position indexed-cursors)
        ;; ascending cursors
        ascending-cursors (map second ascending-indexed-cursors)

        ;; [cursor|nil ...] after splice
        ascending-spliced-cursors (splice-cursors ascending-cursors
                                                  ascending-cursor-ranges-and-replacements)
        ;; original index before sorting to new index after sorting
        index-lookup (into {}
                           (map-indexed (fn [i [oi]] [oi i]))
                           ascending-indexed-cursors)
        ;; spliced cursors in the original unpacked order
        spliced-cursors (mapv (comp ascending-spliced-cursors index-lookup)
                              (range (count ascending-spliced-cursors)))]
    (assert (even? (count spliced-cursors)))
    ;; convert ranges + their new spliced cursors to spliced ranges
    (loop [cursor-range-index 0
           cursor-ranges ascending-cursor-ranges
           spliced-cursor-ranges (transient [])]
      (if-some [cursor-range (first cursor-ranges)]
        (let [cursor-index (* 2 cursor-range-index)
              start (spliced-cursors cursor-index)
              end (spliced-cursors (inc cursor-index))]
          (recur (inc cursor-range-index)
                 (next cursor-ranges)
                 (if-some [cursor-range' (pack-cursor-range cursor-range start end)]
                   (conj! spliced-cursor-ranges cursor-range')
                   spliced-cursor-ranges)))
        (persistent! spliced-cursor-ranges)))))

(defn- splice [lines regions ascending-cursor-ranges-and-replacements]
  ;; Cursor ranges are assumed to be adjusted to lines, and in ascending order.
  ;; The output cursor ranges will also conform to these rules. Note that some
  ;; cursor ranges might have been merged by the splice.
  (when-not (empty? ascending-cursor-ranges-and-replacements)
    (let [invalidated-row (.row (cursor-range-start (ffirst ascending-cursor-ranges-and-replacements)))
          lines' (splice-lines lines ascending-cursor-ranges-and-replacements)
          adjust-cursor-range (partial adjust-cursor-range lines')
          splice-cursor-ranges (fn [ascending-cursor-ranges]
                                 (->> (splice-cursor-ranges ascending-cursor-ranges
                                                            ascending-cursor-ranges-and-replacements)
                                      (map adjust-cursor-range)
                                      merge-cursor-ranges))
          cursor-ranges (map first ascending-cursor-ranges-and-replacements)
          cursor-ranges' (splice-cursor-ranges cursor-ranges)
          replacement-regions (->> (introduce-replacement-ranges ascending-cursor-ranges-and-replacements)
                                   (eduction (map adjust-cursor-range))
                                   merge-cursor-ranges)
          replacement-regions-exist (pos? (count replacement-regions))
          regions' (cond
                     (and regions replacement-regions-exist)
                     (vec (sort (into replacement-regions (splice-cursor-ranges regions))))

                     regions
                     (splice-cursor-ranges regions)

                     replacement-regions-exist
                     replacement-regions)]
      (cond-> {:lines lines'
               :cursor-ranges cursor-ranges'
               :invalidated-row invalidated-row}

              (not= regions regions')
              (assoc :regions regions')))))

(defn- begins-indentation? [grammar ^String line]
  (when (some? line)
    (let [begin? (:begin (:indent grammar))]
      (cond
        (fn? begin?) (begin? line)
        :else (re-find begin? line)))))

(defn- ends-indentation? [grammar ^String line]
  (and (some? line) (some? (some-> grammar :indent :end (re-find line)))))

(defn typing-deindents-line? [grammar lines cursor-ranges typed]
  (if (not= 1 (count cursor-ranges))
    false
    (let [cursor (adjust-cursor lines (CursorRange->Cursor (first cursor-ranges)))
          line (lines (.row cursor))]
      (and (not (ends-indentation? grammar line))
           (ends-indentation? grammar (str (subs line 0 (.col cursor)) typed (subs line (.col cursor))))))))

(defn guess-indent-type [lines ^long tab-spaces]
  (when-not (empty? lines)
    (loop [prev-line-visual-width 0
           sum-visual-width-from-spaces 0
           sum-visual-width-from-tabs 0
           sum-two-space-occurrences 0
           sum-four-space-occurrences 0
           lines lines]
      (if-some [^String line (first lines)]
        (let [line-length (count line)]
          (if-some [[visual-width from-spaces from-tabs] (loop [index 0
                                                                visual-width 0
                                                                from-spaces 0
                                                                from-tabs 0]
                                                           (when (< index line-length)
                                                             (let [character (.charAt line index)]
                                                               (case character
                                                                 \space (recur (inc index) (inc visual-width) (inc from-spaces) from-tabs)
                                                                 \tab (let [visual-width-increase (- tab-spaces ^long (mod visual-width tab-spaces))]
                                                                        (recur (inc index) (+ visual-width visual-width-increase) from-spaces (+ from-tabs visual-width-increase)))
                                                                 [visual-width from-spaces from-tabs]))))]
            ;; Indented line.
            (let [visual-width (long visual-width)
                  indent-width (- visual-width prev-line-visual-width)]
              (recur (long visual-width)
                     (+ sum-visual-width-from-spaces ^long from-spaces)
                     (+ sum-visual-width-from-tabs ^long from-tabs)
                     (case indent-width 2 (inc sum-two-space-occurrences) sum-two-space-occurrences)
                     (case indent-width 4 (inc sum-four-space-occurrences) sum-four-space-occurrences)
                     (next lines)))

            ;; Empty line or nothing but whitespace. Ignore.
            (recur prev-line-visual-width
                   sum-visual-width-from-spaces
                   sum-visual-width-from-tabs
                   sum-two-space-occurrences
                   sum-four-space-occurrences
                   (next lines))))
        (let [uses-spaces? (pos? sum-visual-width-from-spaces)
              uses-tabs? (pos? sum-visual-width-from-tabs)]
          (cond
            (and (not uses-spaces?) (not uses-tabs?))
            nil

            (not uses-spaces?)
            :tabs

            :else
            (let [^double tab-certainty (/ sum-visual-width-from-tabs sum-visual-width-from-spaces)]
              (cond
                ;; Too close to call?
                (< 1.0 tab-certainty 1.1)
                nil

                ;; More indentation from tabs?
                (< 1.0 tab-certainty)
                :tabs

                ;; More indentation from two spaces?
                (< sum-four-space-occurrences sum-two-space-occurrences)
                :two-spaces

                :else
                :four-spaces))))))))

(declare ^:private transform-indentation)

(defn indent-type->indent-string
  ^String [indent-type]
  (case indent-type
    :tabs "\t"
    :two-spaces "  "
    :four-spaces "    "))

(defn indent-type->tab-spaces
  ^long [indent-type]
  (case indent-type
    :two-spaces 2
    (:tabs :four-spaces) 4))

(defn convert-indentation [from-indent-type to-indent-type lines cursor-ranges regions]
  (let [rows (range (count lines))
        from-tab-spaces (indent-type->tab-spaces from-indent-type)
        to-indent-string (indent-type->indent-string to-indent-type)]
    (assoc (transform-indentation rows lines cursor-ranges regions
                                  (fn [^String line]
                                    (let [line-length (count line)
                                          [indent-width text] (loop [index 0
                                                                     visual-width 0]
                                                                (if (<= line-length index)
                                                                  [visual-width nil]
                                                                  (case (.charAt line index)
                                                                    \space (recur (inc index) (inc visual-width))
                                                                    \tab (recur (inc index) (+ visual-width (- from-tab-spaces ^long (mod visual-width from-tab-spaces))))
                                                                    [visual-width (subs line index)])))
                                          indent-level (unchecked-divide-int indent-width from-tab-spaces)
                                          indent-rest (- ^long indent-width (* indent-level from-tab-spaces))]
                                      (str (string/join (repeat indent-level to-indent-string))
                                           (string/join (repeat indent-rest \space))
                                           text))))
      :indent-type to-indent-type)))

(defn indent-level-pattern
  ^Pattern [^long tab-spaces]
  (re-pattern (str "(?:^|\\G)(?:\\t|" (string/join (repeat tab-spaces \space)) ")")))

(defn- parse-indent-level
  ^long [indent-level-pattern line]
  (if (empty? line)
    0
    (let [matcher (re-matcher indent-level-pattern line)]
      (loop [indent-level 0]
        (if (.find matcher)
          (recur (inc indent-level))
          indent-level)))))

(defn- indent-level
  ^long [^long prev-line-indent-level prev-line-begin? line-end?]
  (if (and line-end? prev-line-begin?)
    ;; function foo()
    ;; end|
    prev-line-indent-level

    (if line-end?
      ;; function foo()
      ;;     ...
      ;;     ...
      ;;     end|
      (dec prev-line-indent-level)

      (if prev-line-begin?
        ;; function foo()
        ;; |
        (inc prev-line-indent-level)

        ;; function foo()
        ;;     ...
        ;;     ...
        ;; |
        prev-line-indent-level))))

(defn- indent-line
  ^String [unindented-line indent-string ^long indent-level]
  (string/join (concat (repeat indent-level indent-string) [unindented-line])))

(defn- length-difference-by-row [indentation-splices]
  (into {}
        (keep (fn [[^CursorRange line-cursor-range replacement-lines]]
                (let [row (.row ^Cursor (.from line-cursor-range))
                      old-length (.col ^Cursor (.to line-cursor-range))
                      new-length (count (first replacement-lines))
                      length-difference (- new-length old-length)]
                  (assert (= row (.row ^Cursor (.to line-cursor-range))))
                  (assert (= 1 (count replacement-lines)))
                  (when-not (zero? length-difference)
                    (pair row length-difference)))))
        indentation-splices))

(defn- splice-indentation [lines cursor-ranges regions indentation-splices]
  (when (seq indentation-splices)
    (let [invalidated-row (.row (cursor-range-start (ffirst indentation-splices)))
          lines (splice-lines lines indentation-splices)
          length-difference-by-row (length-difference-by-row indentation-splices)
          splice-cursor-ranges (fn [ascending-cursor-ranges]
                                 (into []
                                       (map (fn [^CursorRange cursor-range]
                                              (let [^Cursor from (.from cursor-range)
                                                    ^Cursor to (.to cursor-range)
                                                    ^long from-col-offset (get length-difference-by-row (.row from) 0)
                                                    ^long to-col-offset (get length-difference-by-row (.row to) 0)]
                                                (if (and (zero? from-col-offset)
                                                         (zero? to-col-offset))
                                                  cursor-range
                                                  (adjust-cursor-range lines (assoc cursor-range
                                                                               :from (offset-cursor from 0 from-col-offset)
                                                                               :to (offset-cursor to 0 to-col-offset)))))))
                                       ascending-cursor-ranges))
          cursor-ranges' (splice-cursor-ranges cursor-ranges)
          regions' (some-> regions splice-cursor-ranges)]
      (cond-> {:lines lines
               :cursor-ranges cursor-ranges'
               :invalidated-row invalidated-row}

              (not= regions regions')
              (assoc :regions regions')))))

(defn- find-indent-level [indent-level-pattern grammar lines ^long queried-row]
  (let [^long start-row (loop [row queried-row]
                          (cond (not (pos? row)) 0
                                (string/blank? (get lines row)) (recur (dec row))
                                :else row))
        start-line (get lines start-row)]
    (loop [row (inc start-row)
           prev-line-begin? (begins-indentation? grammar start-line)
           prev-line-indent-level (parse-indent-level indent-level-pattern start-line)]
      (if (< queried-row row)
        prev-line-indent-level
        (let [line (lines row)
              line-begin? (begins-indentation? grammar line)
              line-end? (ends-indentation? grammar line)
              line-indent-level (indent-level prev-line-indent-level prev-line-begin? line-end?)]
          (recur (inc row)
                 line-begin?
                 line-indent-level))))))

(defn- fix-indentation [affected-cursor-ranges indent-level-pattern indent-string grammar lines cursor-ranges regions]
  (let [fixed-rows (volatile! #{})]
    (splice-indentation lines cursor-ranges regions
                        (into []
                              (mapcat (fn [^CursorRange cursor-range]
                                        (let [start (cursor-range-start cursor-range)
                                              end (cursor-range-end cursor-range)
                                              start-row (.row start)
                                              end-row (inc (.row end))
                                              prev-row (dec start-row)]
                                          (loop [row start-row
                                                 prev-line-indent-level (find-indent-level indent-level-pattern grammar lines prev-row)
                                                 prev-line-begin? (begins-indentation? grammar (get lines prev-row))
                                                 splices (transient [])]
                                            (if (<= end-row row)
                                              (persistent! splices)
                                              (let [next-row (inc row)
                                                    line (lines row)
                                                    line-begin? (begins-indentation? grammar line)
                                                    line-end? (ends-indentation? grammar line)
                                                    line-indent-level (indent-level prev-line-indent-level prev-line-begin? line-end?)]
                                                (if (contains? @fixed-rows row)
                                                  (recur next-row line-indent-level line-begin? splices)
                                                  (let [single-line-edit? (not (cursor-range-multi-line? cursor-range))
                                                        typed? (and single-line-edit? (= 1 (- (.col end) (.col start))))
                                                        line-cursor-range (->CursorRange (->Cursor row 0) (->Cursor row (count line)))
                                                        indented-line (cond
                                                                        ;; Insert typed whitespace (except newlines) without adjusting indentation.
                                                                        (and typed? (whitespace-character-at-index? line (.col start)))
                                                                        line

                                                                        ;; Typing can only decrease indentation (i.e. typing the final character of "end"), never increase it.
                                                                        (and typed? (not (and (ends-indentation? grammar line)
                                                                                              (not (ends-indentation? grammar (str (subs line 0 (.col start))
                                                                                                                                   (subs line (.col end))))))))
                                                                        line

                                                                        ;; On the last line, we don't want to strip any whitespace that comes after the cursor.
                                                                        (= end-row next-row)
                                                                        (let [unindented-line (str (string/triml (subs line 0 (.col end))) (subs line (.col end)))]
                                                                          (indent-line unindented-line indent-string line-indent-level))

                                                                        ;; On in-between lines, we only want to keep whitespace that comes before code.
                                                                        :else
                                                                        (let [unindented-line (string/triml line)]
                                                                          (if (seq unindented-line)
                                                                            (indent-line unindented-line indent-string line-indent-level)
                                                                            "")))
                                                        splices (if (= line indented-line)
                                                                  splices
                                                                  (conj! splices [line-cursor-range [indented-line]]))]
                                                    (vswap! fixed-rows conj row)
                                                    (recur next-row line-indent-level line-begin? splices)))))))))
                              affected-cursor-ranges))))

(defn- transform-indentation [rows lines cursor-ranges regions line-fn]
  (splice-indentation lines cursor-ranges regions
                      (into []
                            (keep (fn [row]
                                    (let [old-line (lines row)
                                          new-line (line-fn old-line)]
                                      (when (not= old-line new-line)
                                        (let [line-cursor-range (->CursorRange (->Cursor row 0) (->Cursor row (count old-line)))]
                                          [line-cursor-range [new-line]])))))
                            rows)))

(defn- fix-indentation-after-splice [{:keys [lines cursor-ranges regions] :as splice-properties} indent-level-pattern indent-string grammar]
  (when (some? splice-properties)
    (assert (vector? lines))
    (assert (vector? cursor-ranges))
    (if (nil? (:indent grammar))
      splice-properties
      (let [indentation-properties (fix-indentation cursor-ranges indent-level-pattern indent-string grammar lines cursor-ranges regions)]
        (merge splice-properties
               (dissoc indentation-properties :invalidated-row))))))

(defn- update-document-width-after-splice [{:keys [lines cursor-ranges] :as splice-properties} ^LayoutInfo pre-splice-layout]
  (when (some? splice-properties)
    (assert (vector? lines))
    (assert (vector? cursor-ranges))
    (let [glyph-metrics (.glyph pre-splice-layout)
          tab-stops (.tab-stops pre-splice-layout)
          old-document-width (or (:document-width splice-properties) (.document-width pre-splice-layout))
          new-document-width (transduce (comp (cursor-ranges->row-runs-xform lines)
                                              (map (fn [[start-row end-row]]
                                                     (max-line-width glyph-metrics tab-stops lines start-row end-row))))
                                        (completing max)
                                        old-document-width
                                        cursor-ranges)]
      (if (not= old-document-width new-document-width)
        (assoc splice-properties :document-width new-document-width)
        splice-properties))))

(defn- insert-lines-seqs [indent-level-pattern indent-string grammar lines cursor-ranges regions ^LayoutInfo layout lines-seqs]
  (when-not (empty? lines-seqs)
    (-> (splice lines regions (map #(pair (adjust-cursor-range lines %1) %2) cursor-ranges lines-seqs))
        (fix-indentation-after-splice indent-level-pattern indent-string grammar)
        (update-document-width-after-splice layout)
        (update :cursor-ranges (partial mapv cursor-range-end-range))
        (frame-cursor layout))))

(defn- insert-text [indent-level-pattern indent-string grammar lines cursor-ranges regions ^LayoutInfo layout ^String text]
  (when-not (empty? text)
    (let [text-lines (util/split-lines text)]
      (insert-lines-seqs indent-level-pattern indent-string grammar lines cursor-ranges regions layout (repeat text-lines)))))

(defn- inc-limited
  ^long [^long n]
  (if (= n Long/MAX_VALUE)
    (inc (- n 1000000))
    (inc n)))

(defn append-distinct-lines [lines regions added-lines line-regions-fn]
  ;; NOTE: line-regions-fn takes a zero-based row index and a line string,
  ;; and is expected to return a seq of ordered regions within the line.
  ;; In addition to these, repeat lines will be encompassed by a :repeat region.
  (assert (vector? lines))
  (assert (vector? regions))
  (let [clean-lines (if (= [""] lines) [] lines)
        [lines' regions'] (loop [added-lines added-lines
                                 lines (transient clean-lines)
                                 line-row (count clean-lines)
                                 regions regions]
                            (if-some [added-line (first added-lines)]
                              (let [prev-line (peek! lines)]
                                (if (= prev-line added-line)
                                  (recur (next added-lines)
                                         lines
                                         line-row
                                         (let [prev-repeat-region-index (util/last-index-where #(= :repeat (:type %)) regions)
                                               prev-repeat-region (some->> prev-repeat-region-index (get regions))
                                               prev-line-row (dec line-row)]
                                           (if (= prev-line-row (some-> prev-repeat-region :from :row))
                                             (assoc regions prev-repeat-region-index (update prev-repeat-region :count inc-limited))
                                             (let [new-repeat-region (let [end-col (count added-line)]
                                                                       (assoc (->CursorRange (->Cursor prev-line-row 0)
                                                                                             (->Cursor prev-line-row end-col))
                                                                         :type :repeat
                                                                         :count 2))]
                                               (util/insert-sort regions new-repeat-region)))))
                                  (recur (next added-lines)
                                         (conj! lines added-line)
                                         (inc line-row)
                                         (into regions (line-regions-fn line-row added-line)))))
                              [(persistent! lines) regions]))
        lines' (if (empty? lines') [""] lines')]
    (cond-> {:lines lines'
             :regions regions'}

            (empty? clean-lines)
            (assoc :invalidated-row 0))))

(defn delete-character-before-cursor [lines grammar auto-closing-parens syntax-info cursor-range]
  (let [cursor (adjust-cursor lines (CursorRange->Cursor cursor-range))]
    (or
      (when-let [{:keys [characters exclude-scopes open-scopes]} (when auto-closing-parens (:auto-insert grammar))]
        ;; auto-remove closing parens only across a single line
        (let [row (.-row cursor)
              ^String line (lines row)
              next-character-index (.-col cursor)
              deleted-character-index (dec next-character-index)]
          ;; check if there exist both at least 1 char to the left and 1 char to
          ;; the right
          (when (and (not (neg? deleted-character-index))
                     (< next-character-index (.length line)))
            (let [deleted-character (.charAt line deleted-character-index)]
              (when-let [closing-counterpart (characters deleted-character)]
                (when (and (= (.charAt line next-character-index)
                              (char closing-counterpart))
                           ;; exclusion: we don't open auto-inserts in certain
                           ;; regions (e.g. inside strings). for these regions, we
                           ;; also don't remove such a matching closing chars...
                           ;; UNLESS the deleted char is white-listed for open
                           (or (nil? exclude-scopes)
                               (not (contains? exclude-scopes (syntax-scope-before-cursor syntax-info grammar (update cursor :col dec))))
                               ;; the "unless" part
                               (when-let [deleted-char-open-scope (get open-scopes deleted-character)]
                                 (= deleted-char-open-scope (syntax-scope-before-cursor syntax-info grammar cursor)))))
                  [(->CursorRange (->Cursor row deleted-character-index)
                                  (->Cursor row (inc next-character-index)))
                   [""]]))))))
      [(->CursorRange cursor (cursor-left lines cursor)) [""]])))

(defn delete-word-before-cursor [lines _grammar _auto-closing-parens _syntax-info cursor-range]
  (let [from (CursorRange->Cursor cursor-range)
        to (cursor-prev-word lines from)]
    [(->CursorRange from to) [""]]))

(defn delete-character-after-cursor [lines _grammar _auto-closing-parens _syntax-info cursor-range]
  (let [from (CursorRange->Cursor cursor-range)
        to (cursor-right lines from)]
    [(->CursorRange from to) [""]]))

(defn delete-word-after-cursor [lines _grammar _auto-closing-parens _syntax-info cursor-range]
  (let [from (CursorRange->Cursor cursor-range)
        to (cursor-next-word lines from)]
    [(->CursorRange from to) [""]]))

(defn- delete-range [lines cursor-range]
  [(adjust-cursor-range lines cursor-range) [""]])

(defn- put-selection-on-clipboard! [lines cursor-ranges clipboard]
  (let [selected-lines-ascending (mapv #(subsequence->lines (cursor-range-subsequence lines %))
                                       cursor-ranges)
        content {clipboard-mime-type-plain-text (string/join "\n" (flatten selected-lines-ascending))
                 clipboard-mime-type-multi-selection selected-lines-ascending}]
    (set-content! clipboard content)))

(defn- can-paste-plain-text? [clipboard]
  (has-content? clipboard clipboard-mime-type-plain-text))

(defn- can-paste-multi-selection? [clipboard cursor-ranges]
  (and (< 1 (count cursor-ranges))
       (has-content? clipboard clipboard-mime-type-multi-selection)))

(defn- compare-adjusted-end-cursors
  ^long [lines ^CursorRange a ^CursorRange b]
  (util/comparisons
    (compare-cursor-position (cursor-range-end (adjust-cursor-range lines a)) (cursor-range-end (adjust-cursor-range lines b)))
    (compare a b))) ; Fallback comparison of unadjusted cursor ranges.

(defn visible-cursor-ranges [lines ^LayoutInfo layout ascending-cursor-ranges]
  (assert (or (nil? ascending-cursor-ranges) (vector? ascending-cursor-ranges)))
  (when-not (empty? ascending-cursor-ranges)
    (let [first-visible-row (.dropped-line-count layout)
          last-visible-row (+ first-visible-row (.drawn-line-count layout))
          not-visible-range (->CursorRange (->Cursor Long/MIN_VALUE Long/MIN_VALUE) (->Cursor first-visible-row 0))
          compare-adjusted-end-cursors (partial compare-adjusted-end-cursors lines)
          binary-search-result (Collections/binarySearch ascending-cursor-ranges not-visible-range compare-adjusted-end-cursors)
          start-index (Math/abs (inc binary-search-result))]
      (assert (<= 0 start-index))
      (into []
            (comp (take-while (partial cursor-range-starts-before-row? last-visible-row))
                  (map (partial adjust-cursor-range lines)))
            (subvec ascending-cursor-ranges start-index)))))

(defn visible-occurrences [lines ^LayoutInfo layout case-sensitive? whole-word? needle-lines]
  (when (not-every? empty? needle-lines)
    (loop [from-cursor (->Cursor (max 0 (- (.dropped-line-count layout) (count needle-lines))) 0)
           visible-cursor-ranges (transient [])]
      (let [matching-cursor-range (find-next-occurrence lines needle-lines from-cursor case-sensitive? whole-word?)]
        (if (and (some? matching-cursor-range)
                 (< (.row (cursor-range-start matching-cursor-range))
                    (+ (.dropped-line-count layout) (.drawn-line-count layout))))
          (recur (cursor-range-end matching-cursor-range)
                 (conj! visible-cursor-ranges matching-cursor-range))
          (persistent! visible-cursor-ranges))))))

(defn visible-occurrences-of-selected-word [lines cursor-ranges ^LayoutInfo layout visible-cursor-ranges]
  (when-some [word-cursor-range (selected-word-cursor-range lines cursor-ranges)]
    (let [subsequence (cursor-range-subsequence lines word-cursor-range)
          needle-lines (subsequence->lines subsequence)
          visible-occurrences (visible-occurrences lines layout false false needle-lines)]
      (filterv (fn [cursor-range]
                 (and (word-cursor-range? lines cursor-range)
                      (not-any? (partial cursor-range-equals? cursor-range) visible-cursor-ranges)))
               visible-occurrences))))

(defn replace-typed-chars [indent-level-pattern indent-string grammar lines regions ^LayoutInfo layout splices]
  (-> (splice lines regions splices)
      (fix-indentation-after-splice indent-level-pattern indent-string grammar)
      (update-document-width-after-splice layout)))

;; -----------------------------------------------------------------------------

(defn scroll [lines scroll-x scroll-y layout ^GestureInfo gesture-start delta-x delta-y]
  ;; Disallow mouse wheel scrolling during middle-button box-selection.
  (when (or (nil? gesture-start)
            (not (and (= :box-selection (.type gesture-start))
                      (= :middle (.button gesture-start)))))
    (let [new-scroll-x (limit-scroll-x layout (+ ^double scroll-x (Math/ceil delta-x)))
          new-scroll-y (limit-scroll-y layout (count lines) (+ ^double scroll-y (Math/ceil delta-y)))]
      (cond-> nil
              (not= scroll-x new-scroll-x) (assoc :scroll-x new-scroll-x)
              (not= scroll-y new-scroll-y) (assoc :scroll-y new-scroll-y)))))

(defn- key-type-with-auto-insert [indent-level-pattern indent-string grammar lines cursor-ranges regions layout syntax-info ^String typed]
  {:pre [(= 1 (.length typed))
         (contains? grammar :auto-insert)]}
  (let [ch (.charAt typed 0)
        {:keys [close-characters characters exclude-scopes close-scopes open-scopes]} (:auto-insert grammar)
        cursor-ranges (mapv (partial adjust-cursor-range lines) cursor-ranges)
        max-range-row (long (transduce (map #(.-row (cursor-range-end %))) max 0 cursor-ranges))
        syntax-info (ensure-syntax-info syntax-info (inc max-range-row) lines grammar)
        changes (-> (splice
                      lines
                      regions
                      (mapv
                        (fn [^CursorRange cursor-range]
                          (cond
                            ;; if no selection and we typed a closing char, e.g. "]"
                            ;; after "[", we only move the cursor to the right
                            (and (cursor-range-empty? cursor-range)
                                 (contains? close-characters ch)
                                 (let [cursor (cursor-range-end cursor-range)
                                       ^String line (lines (.-row cursor))
                                       next-char-index (.-col cursor)]
                                   (and (< next-char-index (.length line))
                                        (let [next-char (.charAt line next-char-index)]
                                          (and (= ch next-char)
                                               ;; Corner case: if the next character is a
                                               ;; closing one, but it resides in an open scope,
                                               ;; skip it here. An example is typing a single
                                               ;; quote (') in this scenario:
                                               ;; 'asd'|'asd'
                                               ;;      ^-- this is a cursor, not a pipe
                                               ;; Without handling the corner case, the cursor
                                               ;; would move right instead of inserting a new
                                               ;; string
                                               (if-let [next-char-open-scope (get open-scopes next-char)]
                                                 (not= next-char-open-scope (syntax-scope-before-cursor syntax-info grammar (update cursor :col inc)))
                                                 true)

                                               ;; exclusion: when we don't open an auto-insert
                                               ;; in certain regions (e.g. inside strings), we
                                               ;; also don't close the regions... UNLESS the
                                               ;; next-char is white-listed for close!
                                               (or (nil? exclude-scopes)
                                                   (not (contains? exclude-scopes (syntax-scope-before-cursor syntax-info grammar cursor)))
                                                   ;; the "unless" part
                                                   (when-let [next-char-close-scope (get close-scopes next-char)]
                                                     (= next-char-close-scope (syntax-scope-before-cursor syntax-info grammar (update cursor :col inc))))))))))
                            (pair (assoc cursor-range :pair ::close) [""])

                            ;; if we insert an open character, also add a closing
                            ;; character after the selection
                            (and (contains? characters ch)
                                 ;; exclusion: we don't open an auto-insert
                                 ;; in certain regions (e.g. inside strings)
                                 (or (nil? exclude-scopes)
                                     (not (contains? exclude-scopes (syntax-scope-before-cursor syntax-info grammar (.-to cursor-range))))))
                            (pair (assoc cursor-range :pair ::open)
                                  (util/split-lines (str ch
                                                         (cursor-range-text lines cursor-range)
                                                         (characters ch))))

                            :else
                            (pair cursor-range (util/split-lines typed))))
                        cursor-ranges))
                    (fix-indentation-after-splice indent-level-pattern indent-string grammar)
                    (update-document-width-after-splice layout))
        {:keys [lines]} changes]
    (-> changes
        (update
          :cursor-ranges
          (fn [cursor-ranges]
            (mapv
              (fn [^CursorRange cursor-range]
                (case (:pair cursor-range)
                  ::close (Cursor->CursorRange (cursor-right lines (cursor-range-end cursor-range)))
                  ::open (let [from (.-from cursor-range)
                               to (.-to cursor-range)
                               range-inverted (pos? (compare-cursor-position from to))
                               start (if range-inverted to from)
                               end (if range-inverted from to)
                               new-start (cursor-right lines start)
                               new-end (cursor-left lines end)]
                           (if range-inverted
                             (->CursorRange new-end new-start)
                             (->CursorRange new-start new-end)))
                  (cursor-range-end-range cursor-range)))
              cursor-ranges)))
        (frame-cursor layout))))

(defn key-typed [indent-level-pattern indent-string grammar auto-closing-parens lines cursor-ranges regions layout syntax-info ^String typed]
  (case typed
    "\r" ; Enter or Return.
    (insert-text indent-level-pattern indent-string grammar lines cursor-ranges regions layout "\n")

    (when (not-any? #(Character/isISOControl ^char %) typed)
      (if (and auto-closing-parens
               (= 1 (.length typed))
               (contains? grammar :auto-insert))
        (key-type-with-auto-insert indent-level-pattern indent-string grammar lines cursor-ranges regions layout syntax-info typed)
        (insert-text indent-level-pattern indent-string grammar lines cursor-ranges regions layout typed)))))

(defn execution-marker? [region]
  (= :execution-marker (:type region)))

(defn execution-marker [lines ^long row location-type]
  (let [line-start (text-start lines row)
        line-end (count (lines row))]
    (assoc (->CursorRange (->Cursor row line-start)
                          (->Cursor row line-end))
           :type :execution-marker
           :location-type location-type)))

(defn make-breakpoint-region [lines ^long row]
  (let [line-start (text-start lines row)
        line-end (count (lines row))]
    (assoc (->CursorRange (->Cursor row line-start)
                          (->Cursor row line-end))
      :type :breakpoint
      :enabled true)))

(defn get-breakpoint-region [regions ^long row]
  (coll/some #(when (and (= (:type %) :breakpoint)
                         (= (:row (:from %)) row))
                %)
             regions))

(defn breakpoint-region? [region]
  (= :breakpoint (:type region)))

(defn breakpoint-row [region]
  (assert (breakpoint-region? region))
  (.row (cursor-range-start region)))

(defn toggle-breakpoint-region [lines regions rows]
  (assert (set? rows))
  (let [removed-rows (into #{} (comp (filter breakpoint-region?) (map breakpoint-row) (filter rows)) regions)
        added-rows (set/difference rows removed-rows)]
    (when-not (and (empty? removed-rows)
                   (empty? added-rows))
      (let [removed-region? (fn [region]
                              (and (breakpoint-region? region)
                                   (contains? removed-rows (breakpoint-row region))))
            regions' (vec (sort (concat (remove removed-region? regions)
                                        (map (partial make-breakpoint-region lines) added-rows))))]
        {:regions regions'}))))

(defn toggle-breakpoint-enabled-region [lines regions rows]
  (assert (set? rows))
  {:regions (mapv (fn [region]
                    (if (and (breakpoint-region? region)
                             (contains? rows (breakpoint-row region)))
                      (assoc region :enabled (not (:enabled region)))
                      region))
                  regions)})

(defn- edit-breakpoint-region [lines regions row]
  {:edited-breakpoint (or (some (fn [region]
                                  (when (and (breakpoint-region? region)
                                             (= row (breakpoint-row region)))
                                    region))
                                regions)
                          (make-breakpoint-region lines row))})

(defn edit-breakpoint-from-single-cursor-range [lines cursor-ranges regions]
  (when (= 1 (count cursor-ranges))
    (let [row (.row ^Cursor (.to ^CursorRange (first cursor-ranges)))]
      (edit-breakpoint-region lines regions row))))

(defn ensure-breakpoint-region [lines regions breakpoint-region]
  {:pre [(breakpoint-region? breakpoint-region)]}
  (let [row (breakpoint-row breakpoint-region)
        filtered-regions (filterv (fn [region]
                                    (not (and (breakpoint-region? region)
                                              (= row (breakpoint-row region)))))
                                  regions)
        clean-breakpoint-region (-> breakpoint-region
                                    (assoc
                                      :from (->Cursor row (text-start lines row))
                                      :to (->Cursor row (count (lines row))))
                                    (cond-> (string/blank? (:condition breakpoint-region))
                                            (dissoc :condition)))]
    {:regions (vec (sort (conj filtered-regions clean-breakpoint-region)))}))

(defn- scroll-y-once [direction ^LayoutInfo layout source-line-count]
  (let [line-height (line-height (.glyph layout))
        ^double scroll-delta (case direction :up (- ^double line-height) :down line-height)
        old-scroll-y (.scroll-y layout)
        scroll-y (limit-scroll-y layout source-line-count (- old-scroll-y scroll-delta))]
    (when (not= scroll-y old-scroll-y)
      {:scroll-y scroll-y})))

(defn tick [lines ^LayoutInfo layout ^GestureInfo gesture-start]
  (when (= :primary (some-> gesture-start :button))
    (case (.type gesture-start)
      ;; Pressing the continuous scroll up area of the vertical scroll bar.
      :scroll-bar-y-hold-up
      (when-some [^Rect r (.scroll-tab-y layout)]
        (when (< (.y gesture-start) (+ (.y r) (* 0.5 (.h r))))
          (scroll-y-once :up layout (count lines))))

      ;; Pressing the continuous scroll down area of the vertical scroll bar.
      :scroll-bar-y-hold-down
      (when-some [^Rect r (.scroll-tab-y layout)]
        (when (> (.y gesture-start) (+ (.y r) (* 0.5 (.h r))))
          (scroll-y-once :down layout (count lines))))

      nil)))

(defn- begin-box-selection [lines ^LayoutInfo layout button click-count x y]
  (let [unadjusted-from-cursor (canvas->cursor layout lines x y)
        from-cursor (adjust-cursor lines unadjusted-from-cursor)]
    {:cursor-ranges [(Cursor->CursorRange from-cursor)]
     :gesture-start (gesture-info :box-selection button click-count x y
                                  :from-doc-x (x->doc-x layout x)
                                  :from-doc-y (y->doc-y layout y))}))

(defn in-gutter? [^LayoutInfo layout x]
  (and (< ^double x (.x ^Rect (.canvas layout)))
       (> ^double x (+ (.x ^Rect (.line-numbers layout)) (.w ^Rect (.line-numbers layout))))))

(defn- y->existing-row [layout lines y]
  (let [clicked-row (y->row layout y)]
    (when (< clicked-row (count lines))
      clicked-row)))

(defn mouse-pressed [lines cursor-ranges regions ^LayoutInfo layout ^LayoutInfo minimap-layout button click-count x y alt-key? shift-key? shortcut-key?]
  (case button
    :primary
    (cond
      ;; Click in the gutter to toggle breakpoints.
      (in-gutter? layout x)
      (when-let [clicked-row (y->existing-row layout lines y)]
        (toggle-breakpoint-region lines regions #{clicked-row}))

      ;; Prepare to drag the horizontal scroll tab.
      (and (not alt-key?) (not shift-key?) (not shortcut-key?) (= 1 click-count) (some-> (.scroll-tab-x layout) (rect-contains? x y)))
      {:gesture-start (gesture-info :scroll-tab-x-drag button click-count x y :scroll-x (.scroll-x layout))}

      ;; Prepare to drag the vertical scroll tab.
      (and (not alt-key?) (not shift-key?) (not shortcut-key?) (= 1 click-count) (some-> (.scroll-tab-y layout) (rect-contains? x y)))
      {:gesture-start (gesture-info :scroll-tab-y-drag button click-count x y :scroll-y (.scroll-y layout))}

      ;; Initiate continuous scroll up.
      (and (not alt-key?) (not shift-key?) (not shortcut-key?)
           (when-some [^Rect r (.scroll-tab-y layout)]
             (and (<= (.x r) x (+ (.x r) (.w r)))
                  (< ^double y (+ (.y r) (* 0.5 (.h r)))))))
      (assoc (scroll-y-once :up layout (count lines))
        :gesture-start (gesture-info :scroll-bar-y-hold-up button click-count x y))

      ;; Initiate continuous scroll down.
      (and (not alt-key?) (not shift-key?) (not shortcut-key?)
           (when-some [^Rect r (.scroll-tab-y layout)]
             (and (<= (.x r) x (+ (.x r) (.w r)))
                  (> ^double y (+ (.y r) (* 0.5 (.h r)))))))
      (assoc (scroll-y-once :down layout (count lines))
        :gesture-start (gesture-info :scroll-bar-y-hold-down button click-count x y))

      ;; Prepare to drag the visible minimap region.
      (and (not alt-key?) (not shift-key?) (not shortcut-key?) (some-> (.minimap layout) (rect-contains? x y)))
      (let [^Rect r (.canvas minimap-layout)
            ^double document-line-height (line-height (.glyph layout))
            ^double minimap-line-height (line-height (.glyph minimap-layout))
            minimap-ratio (/ minimap-line-height document-line-height)
            viewed-start-y (+ (* minimap-ratio (- (.scroll-y layout))) (.scroll-y minimap-layout))
            viewed-height (Math/ceil (* minimap-ratio (.h r)))
            viewed-range-rect (->Rect (.x r) viewed-start-y (.w r) viewed-height)]
        (if (rect-contains? viewed-range-rect x y)
          (when (= 1 click-count)
            {:gesture-start (gesture-info :minimap-viewed-range-drag button click-count x y :scroll-y (.scroll-y layout))})
          (let [clicked-row (y->row minimap-layout y)
                target-cursor (adjust-cursor lines (->Cursor clicked-row 0))]
            (scroll-to-cursor scroll-nowhere scroll-center layout lines target-cursor))))

      ;; Shift-click to extend the existing cursor range.
      (and shift-key? (not alt-key?) (not shortcut-key?) (= 1 click-count) (= 1 (count cursor-ranges)))
      (let [mouse-cursor (adjust-cursor lines (canvas->cursor layout lines x y))
            cursor-range (assoc (peek cursor-ranges) :to mouse-cursor)]
        {:cursor-ranges [cursor-range]
         :gesture-start (gesture-info :cursor-range-selection button click-count x y
                                      :reference-cursor-range cursor-range)})

      ;; Move cursor and prepare for drag-selection.
      (and (not shift-key?) (>= 3 ^long click-count))
      (let [mouse-cursor (adjust-cursor lines (canvas->cursor layout lines x y))
            cursor-range (case (long click-count)
                           1 (Cursor->CursorRange mouse-cursor)
                           2 (word-cursor-range-at-cursor lines mouse-cursor)
                           3 (->CursorRange (->Cursor (.row mouse-cursor) 0) (adjust-cursor lines (->Cursor (inc (.row mouse-cursor)) 0))))
            dragged-cursor-range (assoc cursor-range :dragged true)]
        {:cursor-ranges (if alt-key?
                          (concat-cursor-ranges cursor-ranges [dragged-cursor-range])
                          [dragged-cursor-range])
         :gesture-start (gesture-info :cursor-range-selection button click-count x y
                                      :reference-cursor-range dragged-cursor-range)}))

    :middle
    (cond
      ;; Move cursor and prepare for box selection.
      (and (not alt-key?) (not shift-key?) (not shortcut-key?) (= 1 click-count))
      (begin-box-selection lines layout button click-count x y))

    :secondary
    (cond
      (in-gutter? layout x)
      (when-let [clicked-row (y->existing-row layout lines y)]
        (edit-breakpoint-region lines regions clicked-row))

      (not (some-> (.minimap layout) (rect-contains? x y)))
      (let [mouse-cursor (adjust-cursor lines (canvas->cursor layout lines x y))]
        ;; Move cursor when we are outside of the current selection.
        (when-not (some #(cursor-range-contains? % mouse-cursor) cursor-ranges)
          {:cursor-ranges [(Cursor->CursorRange mouse-cursor)]})))

    :back
    nil
    :forward
    nil))

(defn- mouse-gesture [lines cursor-ranges ^LayoutInfo layout ^LayoutInfo minimap-layout ^GestureInfo gesture-start x y]
  (when (not= :secondary (.button gesture-start))
    (case (.type gesture-start)
      ;; Dragging the horizontal scroll tab.
      :scroll-tab-x-drag
      (let [^double start-scroll (:scroll-x gesture-start)
            screen-to-scroll-ratio (scroll-width-ratio layout)
            screen-delta (- ^double x ^double (:x gesture-start))
            scroll-delta (Math/floor (* screen-delta screen-to-scroll-ratio))
            scroll-x (limit-scroll-x layout (- start-scroll scroll-delta))]
        (when (not= scroll-x (.scroll-x layout))
          {:scroll-x scroll-x}))

      ;; Dragging the vertical scroll tab.
      :scroll-tab-y-drag
      (let [^double start-scroll (:scroll-y gesture-start)
            line-count (count lines)
            screen-to-scroll-ratio (scroll-height-ratio layout line-count)
            screen-delta (- ^double y ^double (:y gesture-start))
            scroll-delta (Math/floor (* screen-delta screen-to-scroll-ratio))
            scroll-y (limit-scroll-y layout line-count (- start-scroll scroll-delta))]
        (when (not= scroll-y (.scroll-y layout))
          {:scroll-y scroll-y}))

      ;; Dragging the visible minimap region.
      :minimap-viewed-range-drag
      (let [^double start-scroll (:scroll-y gesture-start)
            ^double document-line-height (line-height (.glyph layout))
            ^double minimap-line-height (line-height (.glyph minimap-layout))
            line-count (count lines)
            canvas-height (.h ^Rect (.canvas layout))
            document-height (* line-count document-line-height)
            document-scroll-range (- document-height canvas-height)
            minimap-ratio (/ minimap-line-height document-line-height)
            minimap-document-height (* line-count minimap-line-height)
            minimap-visible-document-height (min minimap-document-height canvas-height)
            minimap-canvas-height (* minimap-ratio canvas-height)
            minimap-scroll-range (- minimap-visible-document-height minimap-canvas-height)
            screen-to-scroll-ratio (/ document-scroll-range minimap-scroll-range)
            screen-delta (- ^double y ^double (:y gesture-start))
            scroll-delta (Math/floor (* screen-delta screen-to-scroll-ratio))
            scroll-y (limit-scroll-y layout line-count (- start-scroll scroll-delta))]
        (when (not= scroll-y (.scroll-y layout))
          {:scroll-y scroll-y}))

      ;; Continuous scroll up / down.
      (:scroll-bar-y-hold-up :scroll-bar-y-hold-down)
      {:gesture-start (assoc gesture-start :x x :y y)}

      ;; Box selection.
      :box-selection
      (let [^Rect canvas-rect (.canvas layout)
            from-x (doc-x->x layout (:from-doc-x gesture-start))
            from-y (doc-y->y layout (:from-doc-y gesture-start))
            range-inverted? (< ^double x from-x)
            min-row (y->row layout (min ^double y from-y))
            max-row (y->row layout (max ^double y from-y))
            ^double min-x (if range-inverted? x from-x)
            ^double max-x (if range-inverted? from-x x)
            line-min-x (- min-x (.x canvas-rect) (.scroll-x layout))
            glyph-metrics (.glyph layout)
            tab-stops (.tab-stops layout)
            box-cursor-ranges (into []
                                    (keep (fn [^long row]
                                            (when-some [line (get lines row)]
                                              (when (< line-min-x (line-width glyph-metrics tab-stops line))
                                                (let [min-col (adjust-col lines row (x->col layout min-x line))
                                                      max-col (adjust-col lines row (x->col layout max-x line))
                                                      from-col (if range-inverted? max-col min-col)
                                                      to-col (if range-inverted? min-col max-col)]
                                                  (->CursorRange (->Cursor row from-col)
                                                                 (->Cursor row to-col)))))))
                                    (range min-row (inc max-row)))
            ;; We must have at least one cursor range.
            ;; Use the adjusted mouse cursor if the box includes no characters.
            mouse-cursor (adjust-cursor lines (canvas->cursor layout lines x y))
            new-cursor-ranges (if (seq box-cursor-ranges)
                                box-cursor-ranges
                                [(Cursor->CursorRange mouse-cursor)])
            scroll-properties (when-some [^CursorRange box-cursor-range (first box-cursor-ranges)]
                                (let [scroll-x (:scroll-x (scroll-to-cursor scroll-shortest scroll-shortest layout lines (CursorRange->Cursor box-cursor-range)))
                                      scroll-y (:scroll-y (scroll-to-cursor scroll-shortest scroll-shortest layout lines mouse-cursor))]
                                  (cond-> nil
                                          (some? scroll-x) (assoc :scroll-x scroll-x)
                                          (some? scroll-y) (assoc :scroll-y scroll-y))))]
        (cond-> scroll-properties
                (not= cursor-ranges new-cursor-ranges) (merge {:cursor-ranges new-cursor-ranges
                                                               :hovered-element nil})))

      ;; Drag selection.
      :cursor-range-selection
      (let [^CursorRange reference-cursor-range (:reference-cursor-range gesture-start)
            unadjusted-mouse-cursor (canvas->cursor layout lines x y)
            mouse-cursor (adjust-cursor lines unadjusted-mouse-cursor)
            cursor-range (case (.click-count gesture-start)
                           1 (assoc reference-cursor-range :to mouse-cursor)
                           2 (let [word-cursor-range (word-cursor-range-at-cursor lines mouse-cursor)]
                               (if (cursor-range-midpoint-follows? reference-cursor-range unadjusted-mouse-cursor)
                                 (assoc reference-cursor-range
                                   :from (cursor-range-end reference-cursor-range)
                                   :to (if (= (.col mouse-cursor) (count (lines (.row mouse-cursor))))
                                         mouse-cursor
                                         (min-cursor (cursor-range-start word-cursor-range) (cursor-range-start reference-cursor-range))))
                                 (assoc reference-cursor-range
                                   :from (cursor-range-start reference-cursor-range)
                                   :to (max-cursor (cursor-range-end word-cursor-range) (cursor-range-end reference-cursor-range)))))
                           3 (if (cursor-range-midpoint-follows? reference-cursor-range unadjusted-mouse-cursor)
                               (assoc reference-cursor-range
                                 :from (cursor-range-end reference-cursor-range)
                                 :to (adjust-cursor lines (->Cursor (.row mouse-cursor) 0)))
                               (assoc reference-cursor-range
                                 :from (cursor-range-start reference-cursor-range)
                                 :to (adjust-cursor lines (->Cursor (inc (.row mouse-cursor)) 0)))))
            new-cursor-ranges (merge-cursor-ranges
                                (sort
                                  (into [cursor-range]
                                        (remove :dragged)
                                        cursor-ranges)))]
        (when (not= cursor-ranges new-cursor-ranges)
          (merge {:cursor-ranges new-cursor-ranges
                  :hovered-element nil}
                 (scroll-to-any-cursor layout lines new-cursor-ranges))))
      nil)))

(defn- element-at-position [lines visible-regions ^LayoutInfo layout ^LayoutInfo minimap-layout x y]
  (cond
    ;; Horizontal scroll tab.
    (some-> (.scroll-tab-x layout) (rect-contains? x y))
    {:type :ui-element :ui-element :scroll-tab-x}

    ;; Vertical scroll tab.
    (some-> (.scroll-tab-y layout) (rect-contains? x y))
    {:type :ui-element :ui-element :scroll-tab-y}

    ;; The continuous scroll up area of the vertical scroll bar.
    (when-some [^Rect r (.scroll-tab-y layout)]
      (and (<= (.x r) x (+ (.x r) (.w r)))
           (< ^double y (+ (.y r) (* 0.5 (.h r))))))
    {:type :ui-element :ui-element :scroll-bar-y-up}

    ;; The continuous scroll down area of the vertical scroll bar.
    (when-some [^Rect r (.scroll-tab-y layout)]
      (and (<= (.x r) x (+ (.x r) (.w r)))
           (> ^double y (+ (.y r) (* 0.5 (.h r))))))
    {:type :ui-element :ui-element :scroll-bar-y-down}

    ;; Minimap.
    (some-> (.minimap layout) (rect-contains? x y))
    (let [^Rect r (.canvas minimap-layout)
          ^double document-line-height (line-height (.glyph layout))
          ^double minimap-line-height (line-height (.glyph minimap-layout))
          minimap-ratio (/ minimap-line-height document-line-height)
          viewed-start-y (+ (* minimap-ratio (- (.scroll-y layout))) (.scroll-y minimap-layout))
          viewed-height (Math/ceil (* minimap-ratio (.h r)))
          viewed-range-rect (->Rect (.x r) viewed-start-y (.w r) viewed-height)]
      (if (rect-contains? viewed-range-rect x y)
        {:type :ui-element :ui-element :minimap-viewed-range}
        {:type :ui-element :ui-element :minimap}))

    (in-gutter? layout x)
    {:type :ui-element :ui-element :gutter}

    :else
    (when-some [clickable-region (some (fn [region]
                                         (when (and (some? (:on-click! region))
                                                    (some #(rect-contains? % x y)
                                                          (cursor-range-rects layout lines region)))
                                           region))
                                       visible-regions)]
      {:type :region :region clickable-region})))

(defn- mouse-hover [lines visible-regions ^LayoutInfo layout ^LayoutInfo minimap-layout hovered-element hovered-row x y]
  (let [new-hovered-element (element-at-position lines visible-regions layout minimap-layout x y)
        new-hovered-row (y->row layout y)
        row-changed (not= hovered-row new-hovered-row)
        element-changed (not= hovered-element new-hovered-element)]
    (when (or row-changed element-changed)
      (cond-> {}
        row-changed (assoc :hovered-row new-hovered-row)
        element-changed (assoc :hovered-element new-hovered-element)))))

(defn mouse-moved [lines cursor-ranges visible-regions ^LayoutInfo layout ^LayoutInfo minimap-layout ^GestureInfo gesture-start hovered-element hovered-row x y]
  (if (some? gesture-start)
    (mouse-gesture lines cursor-ranges layout minimap-layout gesture-start x y)
    (mouse-hover lines visible-regions layout minimap-layout hovered-element hovered-row x y)))

(defn mouse-released [lines cursor-ranges visible-regions ^LayoutInfo layout ^LayoutInfo minimap-layout ^GestureInfo gesture-start button hovered-row x y]
  (when (= button (some-> gesture-start :button))
    (assoc (mouse-hover lines visible-regions layout minimap-layout ::force-evaluation hovered-row x y)
      :cursor-ranges (mapv #(dissoc % :dragged) cursor-ranges)
      :gesture-start nil)))

(defn mouse-exited [^GestureInfo gesture-start hovered-element]
  (when (and (nil? gesture-start) (some? hovered-element))
    {:hovered-element nil}))

(defn cut! [lines cursor-ranges regions ^LayoutInfo layout clipboard]
  (when (put-selection-on-clipboard! lines cursor-ranges clipboard)
    (-> (splice lines regions (map (partial delete-range lines) cursor-ranges))
        (frame-cursor layout))))

(defn copy! [lines cursor-ranges clipboard]
  (put-selection-on-clipboard! lines cursor-ranges clipboard)
  nil)

(defn paste [indent-level-pattern indent-string grammar lines cursor-ranges regions ^LayoutInfo layout clipboard]
  (cond
    (can-paste-multi-selection? clipboard cursor-ranges)
    (insert-lines-seqs indent-level-pattern indent-string grammar lines cursor-ranges regions layout (cycle (get-content clipboard clipboard-mime-type-multi-selection)))

    (can-paste-plain-text? clipboard)
    (insert-text indent-level-pattern indent-string grammar lines cursor-ranges regions layout (get-content clipboard clipboard-mime-type-plain-text))))

(defn can-paste? [cursor-ranges clipboard]
  (or (can-paste-plain-text? clipboard)
      (can-paste-multi-selection? clipboard cursor-ranges)))

(defn delete [lines grammar auto-closing-parens syntax-info cursor-ranges regions ^LayoutInfo layout delete-fn]
  (-> (if (every? cursor-range-empty? cursor-ranges)
        (let [syntax-info (if (and auto-closing-parens (:auto-insert grammar))
                            (let [max-row (long (transduce (map #(-> % cursor-range-end .-row)) max 0 cursor-ranges))]
                              (ensure-syntax-info syntax-info (inc max-row) lines grammar))
                            syntax-info)]
          (splice lines regions (map (partial delete-fn lines grammar auto-closing-parens syntax-info) cursor-ranges)))
        (splice lines regions (map (partial delete-range lines) cursor-ranges)))
      (frame-cursor layout)))

(defn- apply-move [lines cursor-ranges ^LayoutInfo layout move-fn cursor-fn]
  (let [new-cursor-ranges (move-fn cursor-ranges cursor-fn lines)]
    (when (not= cursor-ranges new-cursor-ranges)
      (merge {:cursor-ranges new-cursor-ranges}
             (scroll-to-any-cursor layout lines new-cursor-ranges)))))

(defn move [lines cursor-ranges ^LayoutInfo layout move-type cursor-type]
  (if (and (= :navigation move-type)
           (or (= :left cursor-type)
               (= :right cursor-type))
           (not-every? cursor-range-empty? cursor-ranges))
    (case cursor-type
      :left  {:cursor-ranges (mapv cursor-range-start-range cursor-ranges)}
      :right {:cursor-ranges (mapv cursor-range-end-range cursor-ranges)})
    (let [move-fn (move-type->move-fn move-type)
          cursor-fn (case cursor-type
                      :home (partial cursor-home
                                     (every? (partial at-text-start? lines)
                                             (sequence (comp (take 64)
                                                             (map CursorRange->Cursor)
                                                             (map (partial adjust-cursor lines)))
                                                       cursor-ranges)))
                      :end cursor-line-end
                      :up cursor-up
                      :down cursor-down
                      :left cursor-left
                      :right cursor-right
                      :prev-word cursor-prev-word
                      :next-word cursor-next-word
                      :line-start cursor-line-start
                      :line-end cursor-line-end
                      :file-start cursor-file-start
                      :file-end cursor-file-end)]
      (apply-move lines cursor-ranges layout move-fn cursor-fn))))

(defn page-up [lines cursor-ranges scroll-y ^LayoutInfo layout move-type]
  (let [row-count (dec (.drawn-line-count layout))
        ^double line-height (line-height (.glyph layout))
        scroll-height (* row-count line-height)
        move-fn (move-type->move-fn move-type)
        cursor-fn (partial cursor-offset-up row-count)]
    (some-> (apply-move lines cursor-ranges layout move-fn cursor-fn)
            (assoc :scroll-y (limit-scroll-y layout (count lines) (+ ^double scroll-y scroll-height))))))

(defn page-down [lines cursor-ranges scroll-y ^LayoutInfo layout move-type]
  (let [row-count (dec (.drawn-line-count layout))
        ^double line-height (line-height (.glyph layout))
        scroll-height (* row-count line-height)
        move-fn (move-type->move-fn move-type)
        cursor-fn (partial cursor-offset-down row-count)]
    (some-> (apply-move lines cursor-ranges layout move-fn cursor-fn)
            (assoc :scroll-y (limit-scroll-y layout (count lines) (- ^double scroll-y scroll-height))))))

(defn select-next-occurrence [lines cursor-ranges ^LayoutInfo layout]
  ;; If there are one or more non-range cursors, we select the words under the cursors instead of the next occurrence.
  (if-some [replaced-cursor-ranges-by-index (not-empty (into []
                                                             (keep-indexed (fn [index cursor-range]
                                                                             (when (cursor-range-empty? cursor-range)
                                                                               (let [cursor-range' (word-cursor-range-at-cursor lines
                                                                                                                                (->> cursor-range
                                                                                                                                     CursorRange->Cursor
                                                                                                                                     (adjust-cursor lines)))]
                                                                                 [index cursor-range']))))
                                                             cursor-ranges))]
    ;; There are one or more non-range cursors. Select the words under the cursors.
    (let [cursor-ranges' (reduce (fn [cursor-ranges' [index replaced-cursor-range]]
                                   (assoc cursor-ranges' index replaced-cursor-range))
                                 cursor-ranges
                                 replaced-cursor-ranges-by-index)
          focused-cursor-range (peek (peek replaced-cursor-ranges-by-index))
          scroll-properties (scroll-to-cursor-range scroll-shortest scroll-center layout lines focused-cursor-range)]
      (assoc scroll-properties :cursor-ranges cursor-ranges'))

    ;; Every cursor range has one or more characters
    ;; Add next occurrence of the bottom selected range to selection.
    (when-some [^CursorRange needle-cursor-range (peek cursor-ranges)]
      (let [needle-lines (subsequence->lines (cursor-range-subsequence lines needle-cursor-range))
            from-cursor (or (next-occurrence-search-cursor cursor-ranges)
                            (adjust-cursor lines (cursor-range-end (peek cursor-ranges))))]
        (if-some [matching-cursor-range (find-next-occurrence lines needle-lines from-cursor false false)]
          (let [added-cursor-range (if (pos? (compare-cursor-position (.from needle-cursor-range) (.to needle-cursor-range)))
                                     (->CursorRange (.to matching-cursor-range) (.from matching-cursor-range))
                                     matching-cursor-range)
                cursor-ranges' (with-next-occurrence-search-cursor (concat-cursor-ranges cursor-ranges [added-cursor-range])
                                                                   (cursor-range-end added-cursor-range))
                scroll-properties (scroll-to-cursor-range scroll-shortest scroll-center layout lines added-cursor-range)]
            (assoc scroll-properties :cursor-ranges cursor-ranges'))
          (let [cursor-ranges' (with-next-occurrence-search-cursor cursor-ranges (->Cursor 0 0))]
            {:cursor-ranges cursor-ranges'}))))))

(defn split-selection-into-lines [lines cursor-ranges]
  (let [cursor-ranges' (into []
                             (comp (map (partial adjust-cursor-range lines))
                                   (mapcat (fn [^CursorRange cursor-range]
                                             (let [start (cursor-range-start cursor-range)
                                                   end (cursor-range-end cursor-range)]
                                               (if (zero? (- (.row end) (.row start)))
                                                 [(->CursorRange start end)]
                                                 (concat [(->CursorRange start (->Cursor (.row start) (count (lines (.row start)))))]
                                                         (map (fn [^long row] (->CursorRange (->Cursor row 0) (->Cursor row (count (lines row)))))
                                                              (range (inc (.row start)) (.row end)))
                                                         (when (pos? (.col end))
                                                           [(->CursorRange (->Cursor (.row end) 0) end)])))))))
                             cursor-ranges)]
    (when (not= cursor-ranges cursor-ranges')
      {:cursor-ranges cursor-ranges'})))

(defn escape [cursor-ranges]
  (cond
    (< 1 (count cursor-ranges))
    {:cursor-ranges [(first cursor-ranges)]}

    (not (cursor-range-empty? (first cursor-ranges)))
    {:cursor-ranges [(Cursor->CursorRange (CursorRange->Cursor (first cursor-ranges)))]}))

(defn- can-indent? [lines cursor-ranges]
  (or (some? (some cursor-range-multi-line? cursor-ranges))
      (every? (fn [^CursorRange cursor-range]
                (and (cursor-in-leading-whitespace? lines (.from cursor-range))
                     (cursor-in-leading-whitespace? lines (.to cursor-range))))
              cursor-ranges)))

(defn indent [indent-level-pattern indent-string grammar lines cursor-ranges regions ^LayoutInfo layout]
  (if-not (can-indent? lines cursor-ranges)
    (insert-text indent-level-pattern indent-string grammar lines cursor-ranges regions layout indent-string)
    (let [indent-line #(if (empty? %) % (str indent-string %))
          rows (cursor-ranges->rows lines cursor-ranges)]
      (-> (transform-indentation rows lines cursor-ranges regions indent-line)
          (update-document-width-after-splice layout)))))

(defn deindent [lines cursor-ranges regions tab-spaces]
  (let [deindent-pattern (re-pattern (str "^\\t|" (string/join (repeat tab-spaces \space))))
        deindent-line #(string/replace-first % deindent-pattern "")
        rows (cursor-ranges->rows lines cursor-ranges)]
    (transform-indentation rows lines cursor-ranges regions deindent-line)))

(defn reindent [indent-level-pattern indent-string grammar lines cursor-ranges regions ^LayoutInfo layout]
  (let [affected-cursor-ranges (map (fn [cursor-range]
                                      (let [start-row (.row (adjust-cursor lines (cursor-range-start cursor-range)))
                                            end-row (.row (adjust-cursor lines (cursor-range-end cursor-range)))]
                                        (->CursorRange (->Cursor start-row 0) (->Cursor end-row (count (lines end-row))))))
                                    cursor-ranges)]
    (-> (fix-indentation affected-cursor-ranges indent-level-pattern indent-string grammar lines cursor-ranges regions)
        (update-document-width-after-splice layout))))

(defn select-and-frame [lines ^LayoutInfo layout cursor-range]
  (let [adjusted-cursor-range (adjust-cursor-range lines cursor-range)
        scroll-properties (scroll-to-cursor-range scroll-shortest scroll-center layout lines adjusted-cursor-range)]
    (assoc scroll-properties :cursor-ranges [adjusted-cursor-range])))

(defn select-all [lines cursor-ranges]
  (let [row (max 0 (dec (count lines)))
        col (count (get lines row))
        from (->Cursor 0 0)
        to (->Cursor row col)
        cursor-range (->CursorRange from to)]
    (when (or (not= 1 (count cursor-ranges))
              (not (cursor-range-equals? cursor-range (first cursor-ranges))))
      {:cursor-ranges [cursor-range]})))

(defn find-next [lines cursor-ranges ^LayoutInfo layout needle-lines case-sensitive? whole-word? wrap?]
  (let [adjusted-from-cursor (or (next-occurrence-search-cursor cursor-ranges)
                                 (adjust-cursor lines (cursor-range-end (peek cursor-ranges))))]
    (if-some [matching-cursor-range (find-next-occurrence lines needle-lines adjusted-from-cursor case-sensitive? whole-word?)]
      (select-and-frame lines layout matching-cursor-range)
      (when (and wrap? (not= adjusted-from-cursor document-start-cursor))
        (recur lines (with-next-occurrence-search-cursor cursor-ranges document-start-cursor) layout needle-lines case-sensitive? whole-word? wrap?)))))

(defn find-prev [lines cursor-ranges ^LayoutInfo layout needle-lines case-sensitive? whole-word? wrap?]
  (let [adjusted-from-cursor (or (prev-occurrence-search-cursor cursor-ranges)
                                 (adjust-cursor lines (cursor-range-start (peek cursor-ranges))))]
    (if-some [matching-cursor-range (find-prev-occurrence lines needle-lines adjusted-from-cursor case-sensitive? whole-word?)]
      (select-and-frame lines layout matching-cursor-range)
      (when wrap?
        (let [document-end-cursor (document-end-cursor lines)]
          (when (not= adjusted-from-cursor document-end-cursor)
            (recur lines (with-prev-occurrence-search-cursor cursor-ranges document-end-cursor) layout needle-lines case-sensitive? whole-word? wrap?)))))))

(defn- replace-matching-selection [lines cursor-ranges regions ^LayoutInfo layout needle-lines replacement-lines case-sensitive? whole-word?]
  (when (= 1 (count cursor-ranges))
    (let [cursor-range (adjust-cursor-range lines (first cursor-ranges))]
      (when (= cursor-range (find-next-occurrence lines needle-lines (cursor-range-start cursor-range) case-sensitive? whole-word?))
        (-> (splice lines regions [[cursor-range replacement-lines]])
            (update-document-width-after-splice layout)
            (update :cursor-ranges (fn [[cursor-range' :as cursor-ranges']]
                                     (assert (= 1 (count cursor-ranges')))
                                     (-> cursor-ranges'
                                         (with-prev-occurrence-search-cursor (cursor-range-start cursor-range'))
                                         (with-next-occurrence-search-cursor (cursor-range-end cursor-range'))))))))))

(defn replace-next [lines cursor-ranges regions ^LayoutInfo layout needle-lines replacement-lines case-sensitive? whole-word? wrap?]
  (if-some [splice-result (replace-matching-selection lines cursor-ranges regions layout needle-lines replacement-lines case-sensitive? whole-word?)]
    (merge splice-result (find-next (:lines splice-result) (:cursor-ranges splice-result) layout needle-lines case-sensitive? whole-word? wrap?))
    (find-next lines cursor-ranges layout needle-lines case-sensitive? whole-word? wrap?)))

(defn replace-all [lines regions ^LayoutInfo layout needle-lines replacement-lines case-sensitive? whole-word?]
  {:pre [(not= [""] needle-lines)]}
  (-> (splice lines regions
              (loop [from-cursor document-start-cursor
                     splices (transient [])]
                (if-some [matching-cursor-range (find-next-occurrence lines needle-lines from-cursor case-sensitive? whole-word?)]
                  (recur (cursor-range-end matching-cursor-range)
                         (conj! splices [matching-cursor-range replacement-lines]))
                  (persistent! splices))))
      (update-document-width-after-splice layout)))

(defn sort-lines [lines cursor-ranges regions sort-key-fn]
  (let [row-runs (cursor-ranges->row-runs lines (filter cursor-range-multi-line? cursor-ranges))
        sorted-cursor-ranges (map (fn [[^long start-row ^long end-row]]
                                    (let [last-row (dec end-row)]
                                      (->CursorRange (->Cursor start-row 0) (->Cursor last-row (count (lines last-row))))))
                                row-runs)
        splices (map (fn [^CursorRange sorted-cursor-range]
                       (let [sorted-lines (vec (sort-by sort-key-fn (subsequence->lines (cursor-range-subsequence lines sorted-cursor-range))))]
                         [sorted-cursor-range sorted-lines]))
                     sorted-cursor-ranges)]
    (splice lines regions splices)))

(defn- brace-counterpart-info [character]
  (case character
    \{ [\{ \} :next]
    \} [\} \{ :prev]
    \[ [\[ \] :next]
    \] [\] \[ :prev]
    \( [\( \) :next]
    \) [\) \( :prev]
    nil))

(defn- find-brace-at-cursor [lines ^Cursor cursor]
  (let [line (lines (.row cursor))
        [_ _ before-search-direction :as before-cursor-info] (brace-counterpart-info (get line (dec (.col cursor))))
        [_ _ after-search-direction :as after-cursor-info] (brace-counterpart-info (get line (.col cursor)))]
    (cond
      (= :next after-search-direction)
      (conj after-cursor-info (->CursorRange cursor (->Cursor (.row cursor) (inc (.col cursor)))))

      (= :prev before-search-direction)
      (conj before-cursor-info (->CursorRange (->Cursor (.row cursor) (dec (.col cursor))) cursor))

      (some? after-cursor-info)
      (conj after-cursor-info (->CursorRange cursor (->Cursor (.row cursor) (inc (.col cursor)))))

      (some? before-cursor-info)
      (conj before-cursor-info (->CursorRange (->Cursor (.row cursor) (dec (.col cursor))) cursor)))))

(defn- find-brace-counterpart
  ^CursorRange [brace counterpart lines ^Cursor from-cursor step]
  (let [^long step step]
    (assert (not (zero? step)))
    (loop [nesting 0
           row (.row from-cursor)
           col (if (pos? step)
                 (.col from-cursor)
                 (dec (.col from-cursor)))]
      (when-some [line (get lines row)]
        (if-some [character (get line col)]
          (cond
            (= brace character)
            (recur (inc nesting) row (+ step col))

            (and (= counterpart character) (pos? nesting))
            (recur (dec nesting) row (+ step col))

            (and (= counterpart character) (zero? nesting))
            (->CursorRange (->Cursor row col) (->Cursor row (inc col)))

            :else
            (recur nesting row (+ step col)))
          (let [next-row (+ step row)]
            (recur nesting
                   next-row
                   (if (neg? step)
                     (dec (count (get lines next-row)))
                     0))))))))

(defn find-matching-braces [lines ^Cursor cursor]
  (when-some [[brace counterpart search-direction ^CursorRange brace-cursor-range] (find-brace-at-cursor lines cursor)]
    (when-some [counterpart-cursor-range (case search-direction
                                           :prev (find-brace-counterpart brace counterpart lines (.from brace-cursor-range) -1)
                                           :next (find-brace-counterpart brace counterpart lines (.to brace-cursor-range) 1))]
      (pair brace-cursor-range counterpart-cursor-range))))

(defn- line-empty? [line line-start]
  (= line-start (count line)))

(defn- line-commented? [line line-start comment-string]
  (= line-start (string/index-of line comment-string line-start)))

(defn- every-line-commented-or-empty? [lines row+line-starts comment-string]
  (every? (fn [[^long row line-start]]
            (let [line (lines row)]
              (or (line-empty? line line-start)
                  (line-commented? line line-start comment-string))))
          row+line-starts))

(defn- at-least-one-line-commented? [lines row+line-starts comment-string]
  (some (fn [[^long row line-start]]
          (line-commented? (lines row) line-start comment-string))
        row+line-starts))

(defn- min-long-by
  ^long [f coll]
  (reduce #(min ^long %1 ^long (f %2)) Long/MAX_VALUE coll))

(defn- add-comment-regions+replacements [comment-string row+line-starts]
  (let [smallest-common-line-start (min-long-by second row+line-starts)]
    (mapv (fn [[row]]
            [(Cursor->CursorRange (->Cursor row smallest-common-line-start))
             [(str comment-string \space)]])
          row+line-starts)))

(defn- remove-comment-regions+replacements [comment-string lines row+line-starts]
  (let [comment-length (count comment-string)
        smallest-common-whitespace-after-comment
        (min-long-by
          (fn [[row ^long line-start]]
            (let [line (lines row)]
              (if (line-empty? line line-start)
                Long/MAX_VALUE
                (let [line-after-comment (subs line (+ line-start comment-length))]
                  (->> line-after-comment
                       (take-while #(Character/isWhitespace ^char %))
                       (count))))))
          row+line-starts)]
    (into []
          (keep
            (fn [[row ^long line-start]]
              (when-not (line-empty? (lines row) line-start)
                [(->CursorRange (->Cursor row line-start)
                                (->Cursor row (+ line-start
                                                 comment-length
                                                 smallest-common-whitespace-after-comment)))
                 [""]])))
          row+line-starts)))

(defn toggle-comment [lines regions cursor-ranges comment-string]
  (let [rows (cursor-ranges->rows lines cursor-ranges)
        all-regions (vec (sort (into regions
                                     (map #(-> %
                                               (assoc-in [:from ::sticky] :right)
                                               (assoc-in [:to ::sticky] :right)
                                               (assoc ::cursor true)))
                                     cursor-ranges)))
        row+line-starts (mapv (fn [row]
                                [row (text-start lines row)])
                              rows)
        ret (splice lines
                    all-regions
                    (if (and (every-line-commented-or-empty? lines row+line-starts comment-string)
                             (at-least-one-line-commented? lines row+line-starts comment-string))
                      (remove-comment-regions+replacements comment-string lines row+line-starts)
                      (add-comment-regions+replacements comment-string row+line-starts)))
        new-regions (:regions ret all-regions)]
    (assoc ret :regions (filterv (complement ::cursor) new-regions)
               :cursor-ranges (into []
                                    (comp
                                      (filter ::cursor)
                                      (map #(-> %
                                                (update :from dissoc ::sticky)
                                                (update :to dissoc ::sticky)
                                                (dissoc ::cursor))))
                                    new-regions))))

(defn apply-edits
  ([lines regions cursor-ranges ascending-cursor-ranges-and-replacements]
   (let [all-regions (vec (sort (into regions
                                      (map #(-> %
                                                (update :from assoc ::sticky :right)
                                                (update :to assoc ::sticky :right)
                                                (assoc ::cursor true)))
                                      cursor-ranges)))
         ret (splice lines all-regions ascending-cursor-ranges-and-replacements)
         new-regions (:regions ret all-regions)]
     (assoc ret :regions (into [] (remove ::cursor) new-regions)
                :cursor-ranges (into []
                                     (comp
                                       (filter ::cursor)
                                       (map #(-> %
                                                 (update :from dissoc ::sticky)
                                                 (update :to dissoc ::sticky)
                                                 (dissoc ::cursor))))
                                     new-regions))))
  ([lines regions cursor-ranges ascending-cursor-ranges-and-replacements layout]
   (-> (apply-edits lines regions cursor-ranges ascending-cursor-ranges-and-replacements)
       (update-document-width-after-splice layout)
       (frame-cursor layout))))
