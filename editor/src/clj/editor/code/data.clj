(ns editor.code.data
  (:require [clojure.set :as set]
            [clojure.string :as string]
            [editor.code.syntax :as syntax]
            [editor.code.util :as util])
  (:import (java.io IOException Reader Writer)
           (java.nio CharBuffer)
           (java.util.regex MatchResult Pattern)))

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

(defrecord Cursor [^long row ^long col]
  Comparable
  (compareTo [_this other]
    (let [row-comparison (compare row (.row ^Cursor other))]
      (if (zero? row-comparison)
        (compare col (.col ^Cursor other))
        row-comparison))))

(defmethod print-method Cursor [^Cursor c, ^Writer w]
  (.write w (pr-str (merge {:Cursor [(.row c) (.col c)]}
                           (dissoc c :row :col)))))

(defn- cursor-in-leading-whitespace? [lines ^Cursor cursor]
  (let [line (lines (.row cursor))]
    (if (and (zero? (.col cursor))
             (zero? (count line)))
      false
      (every? (partial whitespace-character-at-index? line)
              (range 0 (.col cursor))))))

(declare cursor-range-start cursor-range-end)

(defrecord CursorRange [^Cursor from ^Cursor to]
  Comparable
  (compareTo [this other]
    (let [start-comparison (compare (cursor-range-start this) (cursor-range-start other))]
      (if (zero? start-comparison)
        (let [end-comparison (compare (cursor-range-end this) (cursor-range-end other))]
          (if (zero? end-comparison)
            (let [from-comparison (compare from (.from ^CursorRange other))]
              (if (zero? from-comparison)
                (compare to (.to ^CursorRange other))
                from-comparison))
            end-comparison))
        start-comparison))))

(defmethod print-method CursorRange [^CursorRange cr, ^Writer w]
  (let [^Cursor from (.from cr)
        ^Cursor to (.to cr)]
    (.write w (pr-str (merge {:CursorRange [[(.row from) (.col from)]
                                            [(.row to) (.col to)]]}
                             (dissoc cr :from :to))))))

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
  (if (neg? (compare a b)) a b))

(defn- max-cursor
  ^Cursor [^Cursor a ^Cursor b]
  (if (pos? (compare a b)) a b))

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

(defn cursor-range-contains? [^CursorRange cursor-range ^Cursor cursor]
  ;; NOTE: Returns true when the cursor is on either edge of the cursor range.
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

(defn cursor-ranges->rows [cursor-ranges]
  (into (sorted-set) (map #(.row (CursorRange->Cursor %))) cursor-ranges))

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
                       glyph
                       tab-stops
                       scroll-tab-y
                       ^double scroll-x
                       ^double scroll-y
                       ^double scroll-y-remainder
                       ^long drawn-line-count
                       ^long dropped-line-count])

(defn- next-tab-stop-x
  ^double [tab-stops ^double x]
  (some (fn [^double tab-stop]
          (when (< x tab-stop)
            tab-stop))
        tab-stops))

(defn advance-text
  "Calculates the x position where text would end if drawn at the specified start x position in the document.
  This is not as simple as one would think, as tab stops and full-width characters must be taken into account."
  [^LayoutInfo layout ^String text start-index end-index start-x]
  (let [glyph-metrics (.glyph layout)]
    (loop [^long index start-index
           ^double x start-x]
      (if (= ^long end-index index)
        x
        (let [glyph (.charAt text index)
              next-x (case glyph
                       \tab (next-tab-stop-x (.tab-stops layout) x)
                       (+ x ^double (char-width glyph-metrics glyph)))]
          (recur (inc index) next-x))))))

(defn line-width
  "Helper function that provides an accurate line width measurement, taking tab stops into account."
  ^double [^LayoutInfo layout ^String line]
  (advance-text layout line 0 (count line) 0.0))

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

(defn- visible-height-ratio
  ^double [^LayoutInfo layout ^long line-count]
  (let [line-height (line-height (.glyph layout))
        document-height (* line-count ^double line-height)
        visible-height (.h ^Rect (.canvas layout))
        visible-ratio (min 1.0 (/ visible-height document-height))]
    visible-ratio))

(defn- scroll-tab-y-rect
  ^Rect [^Rect canvas-rect line-height source-line-count dropped-line-count scroll-y-remainder]
  (let [document-height (* ^double source-line-count ^double line-height)
        visible-height (.h canvas-rect)
        visible-ratio (/ visible-height document-height)]
    (when (< visible-ratio 1.0)
      (let [visible-top (- (* ^double dropped-line-count ^double line-height) ^double scroll-y-remainder)
            scroll-bar-width 14.0
            scroll-bar-height (- (.h canvas-rect) (.y canvas-rect))
            scroll-bar-left (- (+ (.x canvas-rect) (.w canvas-rect)) scroll-bar-width)
            scroll-bar-top (.y canvas-rect)
            scroll-tab-height (* scroll-bar-height (min 1.0 visible-ratio))
            scroll-tab-top (+ scroll-bar-top (* (/ visible-top document-height) scroll-bar-height))]
        (->Rect scroll-bar-left scroll-tab-top scroll-bar-width scroll-tab-height)))))

(defn layout-info
  ^LayoutInfo [canvas-width canvas-height scroll-x scroll-y source-line-count glyph-metrics tab-spaces]
  (let [^double line-height (line-height glyph-metrics)
        dropped-line-count (long (/ ^double scroll-y (- line-height)))
        scroll-y-remainder (double (mod ^double scroll-y (- line-height)))
        drawn-line-count (long (Math/ceil (/ ^double (- ^double canvas-height scroll-y-remainder) line-height)))
        max-line-number-width (Math/ceil (* ^double (char-width glyph-metrics \0) (count (str source-line-count))))
        gutter-margin (Math/ceil (+ 0.0 line-height))
        gutter-width (+ gutter-margin max-line-number-width gutter-margin)
        line-numbers-rect (->Rect gutter-margin 0.0 max-line-number-width canvas-height)
        canvas-rect (->Rect gutter-width 0.0 (- ^double canvas-width gutter-width) canvas-height)
        scroll-tab-y-rect (scroll-tab-y-rect canvas-rect line-height source-line-count dropped-line-count scroll-y-remainder)
        ^double space-width (char-width glyph-metrics \space)
        tab-width (* space-width (double tab-spaces))
        tab-stops (iterate (partial + tab-width) tab-width)]
    (->LayoutInfo line-numbers-rect
                  canvas-rect
                  glyph-metrics
                  tab-stops
                  scroll-tab-y-rect
                  scroll-x
                  scroll-y
                  scroll-y-remainder
                  drawn-line-count
                  dropped-line-count)))

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
  (let [line-x (- x
                  (.x ^Rect (.canvas layout))
                  (.scroll-x layout))
        line-length (count line)
        glyph-metrics (.glyph layout)]
    (loop [col 0
           start-x 0.0]
      (if (<= line-length col)
        col
        (let [next-col (inc col)
              end-x (double (advance-text layout line col next-col start-x))]
          (if (<= end-x line-x)
            (recur next-col end-x)
            (max 0 (+ col (long (+ 0.5 (/ (- line-x start-x) (- end-x start-x))))))))))))

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

(defn canvas->cursor
  ^Cursor [^LayoutInfo layout lines x y]
  (let [row (y->row layout y)
        line (get lines (adjust-row lines row))
        col (x->col layout x line)]
    (->Cursor row col)))

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
  (or (::prev-occurrence-search-cursor (meta cursor-ranges))
      (cursor-range-start (first cursor-ranges))))

(defn- with-prev-occurrence-search-cursor [cursor-ranges cursor]
  (assert (instance? Cursor cursor))
  (with-meta cursor-ranges {::prev-occurrence-search-cursor cursor}))

(defn- next-occurrence-search-cursor
  ^Cursor [cursor-ranges]
  (or (::next-occurrence-search-cursor (meta cursor-ranges))
      (cursor-range-end (peek cursor-ranges))))

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
  ^CursorRange [haystack-lines needle-lines ^Cursor from-cursor case-sensitive? whole-word?]
  (let [needle-patterns (needle-patterns needle-lines case-sensitive? whole-word?)]
    (loop [cursor from-cursor
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
        (when (or (not whole-word?)
                  (word-boundary-after-index? (haystack-lines (.row from-cursor)) (.col from-cursor)))
          matched-range)))))

(defn find-next-occurrence
  ^CursorRange [haystack-lines needle-lines ^Cursor from-cursor case-sensitive? whole-word?]
  (let [needle-patterns (needle-patterns needle-lines case-sensitive? whole-word?)]
    (loop [cursor from-cursor
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
        (when (or (not whole-word?)
                  (word-boundary-before-index? (haystack-lines (.row from-cursor)) (.col from-cursor)))
          matched-range)))))

(defn find-sequential-words-in-scope [haystack-lines needle-words ^CursorRange scope]
  (let [scope-end (cursor-range-end scope)]
    (loop [from-cursor (cursor-range-start scope)
           words needle-words
           matching-cursor-ranges (transient [])]
      (if-some [word (first words)]
        (if-some [matching-cursor-range (find-next-occurrence haystack-lines [word] from-cursor true false)]
          (if (neg? (compare scope-end (cursor-range-end matching-cursor-range)))
            (persistent! matching-cursor-ranges)
            (recur (cursor-range-end matching-cursor-range)
                   (next words)
                   (conj! matching-cursor-ranges matching-cursor-range)))
          (recur from-cursor
                 (next words)
                 matching-cursor-ranges))
        (persistent! matching-cursor-ranges)))))

(defn- word-cursor-range-at-cursor
  ^CursorRange [lines ^Cursor cursor]
  (let [line (lines (.row cursor))
        line-length (count line)
        on-whitespace? (and (whitespace-character-at-index? line (.col cursor))
                            (or (zero? (.col cursor))
                                (whitespace-character-at-index? line (dec (.col cursor)))))
        same-word? (if on-whitespace?
                     (partial whitespace-character-at-index? line)
                     (partial identifier-character-at-index? line))
        col (if (or (= line-length (.col cursor))
                    (not (same-word? (.col cursor))))
              (max 0 (dec (.col cursor)))
              (.col cursor))
        start-col (inc (or (long (first (drop-while same-word? (iterate dec col)))) -1))
        end-col (or (first (drop-while same-word? (iterate inc col))) line-length)]
    (->CursorRange (->Cursor (.row cursor) start-col)
                   (->Cursor (.row cursor) end-col))))

(defn word-cursor-range? [lines ^CursorRange adjusted-cursor-range]
  (let [cursor (cursor-range-start adjusted-cursor-range)
        word-cursor-range (word-cursor-range-at-cursor lines cursor)]
    (when (and (cursor-range-equals? word-cursor-range adjusted-cursor-range)
               (not (Character/isWhitespace (.charAt ^String (lines (.row cursor)) (.col cursor)))))
      adjusted-cursor-range)))

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

(defn limit-scroll-y
  ^double [^LayoutInfo layout lines ^double scroll-y]
  (let [line-count (count lines)
        ^double line-height (line-height (.glyph layout))
        document-height (* line-count line-height)
        canvas-height (.h ^Rect (.canvas layout))
        scroll-min (- canvas-height document-height)]
    (min 0.0 (max scroll-y scroll-min))))

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

(defn- scroll-to-rect [scroll-x-fn scroll-y-fn ^LayoutInfo layout lines ^Rect target-rect]
  (let [canvas-rect ^Rect (.canvas layout)
        margin-x (line-height (.glyph layout))
        margin-y 0.0
        scroll-x (or (scroll-x-fn margin-x (.x canvas-rect) (.w canvas-rect) (.x target-rect) (.w target-rect) (.scroll-x layout)) (.scroll-x layout))
        scroll-x (Math/floor scroll-x)
        scroll-y (or (scroll-y-fn margin-y (.y canvas-rect) (.h canvas-rect) (.y target-rect) (.h target-rect) (.scroll-y layout)) (.scroll-y layout))
        scroll-y (limit-scroll-y layout lines (Math/floor scroll-y))]
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
  (let [y-comparison (compare (scroll-distance (.scroll-y layout) (:scroll-y a))
                              (scroll-distance (.scroll-y layout) (:scroll-y b)))]
    (if (zero? y-comparison)
      (compare (scroll-distance (.scroll-x layout) (:scroll-x a))
               (scroll-distance (.scroll-x layout) (:scroll-x b)))
      y-comparison)))

(defn scroll-to-any-cursor [^LayoutInfo layout lines cursor-ranges]
  (reduce (fn [shortest-scroll scroll]
            (cond (nil? scroll) (reduced nil) ;; Early-out: No scroll required.
                  (neg? (compare-scroll-severity layout scroll shortest-scroll)) scroll
                  :else shortest-scroll))
          (sequence (comp (map CursorRange->Cursor)
                          (map (partial adjust-cursor lines))
                          (map (partial scroll-to-cursor scroll-shortest scroll-shortest layout lines)))
                    cursor-ranges)))

(defn frame-cursor [{:keys [lines cursor-ranges] :as props} ^LayoutInfo layout]
  (assert (vector? lines))
  (assert (vector? cursor-ranges))
  (merge props (scroll-to-any-cursor layout lines cursor-ranges)))

(defn- ensure-syntax-info [syntax-info ^long end-row lines grammar]
  (let [valid-count (count syntax-info)]
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

(defn highlight-visible-syntax [lines syntax-info ^LayoutInfo layout grammar]
  (let [start-line (.dropped-line-count layout)
        end-line (min (count lines) (+ start-line (.drawn-line-count layout)))]
    (ensure-syntax-info syntax-info end-line lines grammar)))

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
          range-inverted? (pos? (compare from to))
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
        as->bs (compare as bs)
        ae->be (compare ae be)]

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
          (pos? (compare ae bs))
          ;;  A>   B>  <A   <B
          (if (neg? (compare (.from b) (.to b)))
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
          (pos? (compare be as))
          ;;  B>   A>  <B   <A
          (if (neg? (compare (.from b) (.to b)))
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

(defn- splice-lines [lines ascending-cursor-ranges-and-replacements]
  (into []
        cat
        (loop [start (->Cursor 0 0)
               rest ascending-cursor-ranges-and-replacements
               lines-seqs (transient [[""]])]
          (if-some [[cursor-range replacement-lines] (first rest)]
            (let [prior-end (adjust-cursor lines (cursor-range-start cursor-range))]
              (recur (cursor-range-end cursor-range)
                     (next rest)
                     (cond-> lines-seqs
                             (neg? (compare start prior-end)) (append-subsequence! (cursor-range-subsequence lines (->CursorRange start prior-end)))
                             (seq replacement-lines) (append-subsequence! (lines->subsequence replacement-lines)))))
            (let [end (->Cursor (dec (count lines)) (count (peek lines)))
                  end-seq (cursor-range-subsequence lines (->CursorRange start end))]
              (persistent! (if (empty-subsequence? end-seq)
                             lines-seqs
                             (append-subsequence! lines-seqs end-seq))))))))

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
        range-inverted? (pos? (compare from to))
        start (assoc (if range-inverted? to from) :order :start)
        end (assoc (if range-inverted? from to) :order :end)]
    (util/pair start end)))

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

(defn- splice-cursors [ascending-cursors ascending-cursor-ranges-and-replacements]
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
            (let [start-comparison (compare cursor (.start splice-info))]
              (or (neg? start-comparison)
                  (and (= :start (:order cursor)) (zero? start-comparison))))
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

            (neg? (compare cursor (.end splice-info)))
            ;; The cursor is inside the splice.
            ;; Append a nil cursor to flag the range for removal and move on to the next cursor.
            (recur prev-splice-info
                   splice-info
                   rest-splices
                   (next cursors)
                   (conj! spliced-cursors nil))

            :else
            ;; The cursor is after the splice end.
            ;; Accumulate offset from splice and move on to the next splice.
            (recur splice-info
                   (accumulate-splice splice-info (first rest-splices))
                   (next rest-splices)
                   cursors
                   spliced-cursors)))))))

(defn- splice-cursor-ranges [ascending-cursor-ranges ascending-cursor-ranges-and-replacements]
  (let [cursors (sequence (mapcat unpack-cursor-range) ascending-cursor-ranges)
        indexed-cursors (map-indexed vector cursors)
        ascending-indexed-cursors (sort-by second indexed-cursors)
        ascending-cursors (map second ascending-indexed-cursors)
        ascending-spliced-cursors (splice-cursors ascending-cursors ascending-cursor-ranges-and-replacements)
        index-lookup (into {} (map-indexed (fn [i [oi]] [oi i])) ascending-indexed-cursors)
        spliced-cursors (mapv (comp ascending-spliced-cursors index-lookup) (range (count ascending-spliced-cursors)))]
    (assert (even? (count spliced-cursors)))
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
  ;; The output cursor ranges will also confirm to these rules. Note that some
  ;; cursor ranges might have been merged by the splice.
  (when-not (empty? ascending-cursor-ranges-and-replacements)
    (let [invalidated-row (.row (cursor-range-start (ffirst ascending-cursor-ranges-and-replacements)))
          lines' (splice-lines lines ascending-cursor-ranges-and-replacements)
          adjust-cursor-range (partial adjust-cursor-range lines')
          splice-cursor-ranges (fn [ascending-cursor-ranges]
                                 (merge-cursor-ranges (map adjust-cursor-range
                                                           (splice-cursor-ranges ascending-cursor-ranges ascending-cursor-ranges-and-replacements))))
          cursor-ranges (map first ascending-cursor-ranges-and-replacements)
          cursor-ranges' (splice-cursor-ranges cursor-ranges)
          regions' (some-> regions splice-cursor-ranges)]
      (cond-> {:lines lines'
               :cursor-ranges cursor-ranges'
               :invalidated-row invalidated-row}

              (not= regions regions')
              (assoc :regions regions')))))

(defn indent-level-pattern
  ^Pattern [^long tab-spaces]
  (re-pattern (str "(?:^|\\G)(?:\\t|" (string/join (repeat tab-spaces \space)) ")")))

(defn- parse-indentation [indent-level-pattern line]
  (if (nil? line)
    (util/pair 0 0)
    (let [matcher (re-matcher indent-level-pattern line)]
      (loop [indent-level 0
             indent-end 0]
        (if (.find matcher)
          (recur (inc indent-level) (.end matcher))
          (util/pair indent-level indent-end))))))

(defn- indent-level
  ^long [^long prev-line-indent-level prev-line-begin? line-end?]
  (if (and line-end? prev-line-begin?)
    ;; function foo()
    ;; end|
    prev-line-indent-level

    (if line-end?
      ;; if foo
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
  ^String [line indent-string ^long indent-level]
  (string/join (concat (repeat indent-level indent-string) [line])))

(defn- indented-splice [indent-level-pattern indent-string grammar lines cursor-range replacement-lines]
  (assert (seq replacement-lines))
  (let [{:keys [begin end]} (:indent grammar)
        adjusted-cursor-range (adjust-cursor-range lines cursor-range)
        insert-cursor (cursor-range-start adjusted-cursor-range)
        modified-line (str (subs (get lines (.row insert-cursor)) 0 (.col insert-cursor)) (first replacement-lines))
        modified-line-indent-level (first (parse-indentation indent-level-pattern modified-line))
        modified-line-begin? (some? (some-> begin (re-find modified-line)))
        indented-replacement-lines (loop [lines (next replacement-lines)
                                          prev-line-indent-level modified-line-indent-level
                                          prev-line-begin? modified-line-begin?
                                          indented-lines (transient [(first replacement-lines)])]
                                     (if-some [line (some-> lines first string/triml)]
                                       (let [line-begin? (some? (some-> begin (re-find line)))
                                             line-end? (some? (some-> end (re-find line)))
                                             line-indent-level (indent-level prev-line-indent-level prev-line-begin? line-end?)]
                                         (recur (next lines)
                                                line-indent-level
                                                line-begin?
                                                (conj! indented-lines
                                                       (indent-line line indent-string line-indent-level))))
                                       (persistent! indented-lines)))]
    [adjusted-cursor-range indented-replacement-lines]))

(defn- insert-lines-seqs [indent-level-pattern indent-string grammar lines cursor-ranges regions ^LayoutInfo layout lines-seqs]
  (when-not (empty? lines-seqs)
    (-> (splice lines regions (map (partial indented-splice indent-level-pattern indent-string grammar lines) cursor-ranges lines-seqs))
        (update :cursor-ranges (partial mapv cursor-range-end-range))
        (frame-cursor layout))))

(defn- insert-text [indent-level-pattern indent-string grammar lines cursor-ranges regions ^LayoutInfo layout ^String text]
  (when-not (empty? text)
    (let [text-lines (util/split-lines text)]
      (insert-lines-seqs indent-level-pattern indent-string grammar lines cursor-ranges regions layout (repeat text-lines)))))

(defn delete-character-before-cursor [lines cursor-range]
  (let [from (CursorRange->Cursor cursor-range)
        to (cursor-left lines from)]
    [(->CursorRange from to) [""]]))

(defn delete-character-after-cursor [lines cursor-range]
  (let [from (CursorRange->Cursor cursor-range)
        to (cursor-right lines from)]
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

(defn visible-cursor-ranges [lines ^LayoutInfo layout cursor-ranges]
  (into []
        (comp (drop-while (partial cursor-range-ends-before-row? (.dropped-line-count layout)))
              (take-while (partial cursor-range-starts-before-row? (+ (.dropped-line-count layout) (.drawn-line-count layout))))
              (map (partial adjust-cursor-range lines)))
        cursor-ranges))

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

(defn replace-typed-chars [indent-level-pattern indent-string grammar lines cursor-ranges regions replaced-char-count replacement-lines]
  (assert (not (neg? ^long replaced-char-count)))
  (let [splices (mapv (fn [^CursorRange cursor-range]
                        (let [adjusted-cursor-range (adjust-cursor-range lines cursor-range)
                              start (cursor-range-start adjusted-cursor-range)
                              new-start-col (- (.col start) ^long replaced-char-count)
                              new-start (->Cursor (.row start) new-start-col)
                              end (cursor-range-end adjusted-cursor-range)]
                          (assert (not (neg? new-start-col)))
                          (indented-splice indent-level-pattern indent-string grammar lines (->CursorRange new-start end) replacement-lines)))
                      cursor-ranges)]
    (splice lines regions splices)))

;; -----------------------------------------------------------------------------

(defn scroll [lines scroll-x scroll-y layout delta-x delta-y]
  (let [new-scroll-x (min 0.0 (+ ^double scroll-x (Math/ceil delta-x)))
        new-scroll-y (limit-scroll-y layout lines (+ ^double scroll-y (Math/ceil delta-y)))]
    (cond-> nil
            (not= scroll-x new-scroll-x) (assoc :scroll-x new-scroll-x)
            (not= scroll-y new-scroll-y) (assoc :scroll-y new-scroll-y))))

(defn- transform-indentation [lines cursor-ranges regions line-fn]
  (let [invalidated-row (.row (cursor-range-start (first cursor-ranges)))
        splices (into []
                      (comp (map (partial adjust-cursor-range lines))
                            (mapcat (fn [^CursorRange cursor-range]
                                      (range (.row (cursor-range-start cursor-range))
                                             (inc (.row (cursor-range-end cursor-range))))))
                            (distinct)
                            (keep (fn [row]
                                    (let [old-line (lines row)
                                          new-line (line-fn old-line)]
                                      (when (not= old-line new-line)
                                        (let [line-cursor-range (->CursorRange (->Cursor row 0) (->Cursor row (count old-line)))
                                              length-difference (- (count new-line) (count old-line))]
                                          [line-cursor-range [new-line] length-difference]))))))
                      cursor-ranges)
        lines (splice-lines lines splices)
        length-difference-by-row (into {}
                                       (keep (fn [[^CursorRange line-cursor-range _ ^long length-difference]]
                                               (when-not (zero? length-difference)
                                                 [(.row (CursorRange->Cursor line-cursor-range)) length-difference])))
                                       splices)
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
                                                (assoc cursor-range
                                                  :from (offset-cursor from 0 from-col-offset)
                                                  :to (offset-cursor to 0 to-col-offset))))))
                                     ascending-cursor-ranges))
        cursor-ranges' (splice-cursor-ranges cursor-ranges)
        regions' (some-> regions splice-cursor-ranges)]
    (cond-> {:lines lines
             :cursor-ranges cursor-ranges'
             :invalidated-row invalidated-row}

            (not= regions regions')
            (assoc :regions regions'))))

(defn- deindent-lines [lines cursor-ranges regions tab-spaces]
  (let [deindent-pattern (re-pattern (str "^\\t|" (string/join (repeat tab-spaces \space))))
        deindent-line #(string/replace-first % deindent-pattern "")]
    (transform-indentation lines cursor-ranges regions deindent-line)))

(defn- indent-lines [lines cursor-ranges regions indent-string]
  (let [indent-line #(if (empty? %) % (str indent-string %))]
    (transform-indentation lines cursor-ranges regions indent-line)))

(defn- can-indent? [lines cursor-ranges]
  (or (some? (some cursor-range-multi-line? cursor-ranges))
      (every? (fn [^CursorRange cursor-range]
                (and (cursor-in-leading-whitespace? lines (.from cursor-range))
                     (cursor-in-leading-whitespace? lines (.to cursor-range))))
              cursor-ranges)))

(defn key-typed [indent-level-pattern indent-string grammar lines cursor-ranges regions layout typed meta-key? control-key?]
  (when-not (or meta-key? control-key?)
    (case typed
      "\r" ; Enter or Return.
      (insert-text indent-level-pattern indent-string grammar lines cursor-ranges regions layout "\n")

      (when (not-any? #(Character/isISOControl ^char %) typed)
        (insert-text indent-level-pattern indent-string grammar lines cursor-ranges regions layout typed)))))

(defn breakpoint [lines ^long row]
  (let [line-start (text-start lines row)
        line-end (count (lines row))]
    (assoc (->CursorRange (->Cursor row line-start)
                          (->Cursor row line-end))
      :type :breakpoint)))

(defn breakpoint-region? [region]
  (= :breakpoint (:type region)))

(defn breakpoint-row [region]
  (assert (breakpoint-region? region))
  (.row (cursor-range-start region)))

(defn toggle-breakpoint [lines regions rows]
  (assert (set? rows))
  (let [removed-rows (into #{} (comp (filter breakpoint-region?) (map breakpoint-row) (filter rows)) regions)
        added-rows (set/difference rows removed-rows)]
    (when-not (and (empty? removed-rows)
                   (empty? added-rows))
      (let [removed-region? (fn [region]
                              (and (breakpoint-region? region)
                                   (contains? removed-rows (breakpoint-row region))))
            regions' (vec (sort (concat (remove removed-region? regions)
                                        (map (partial breakpoint lines) added-rows))))]
        {:regions regions'}))))

(defn mouse-pressed [lines cursor-ranges regions ^LayoutInfo layout button click-count x y alt-key? shift-key? shortcut-key?]
  (when (= :primary button)
    (cond
      ;; Click in the gutter to toggle breakpoints.
      (and (< ^double x (.x ^Rect (.canvas layout)))
           (> ^double x (+ (.x ^Rect (.line-numbers layout)) (.w ^Rect (.line-numbers layout)))))
      (let [clicked-row (y->row layout y)]
        (when (< clicked-row (count lines))
          (toggle-breakpoint lines regions #{clicked-row})))

      ;; Prepare to drag the scroll tab.
      (and (not alt-key?) (not shift-key?) (not shortcut-key?) (= 1 click-count) (some-> (.scroll-tab-y layout) (rect-contains? x y)))
      {:gesture-start (gesture-info :scroll-tab-y-drag button click-count x y :scroll-y (.scroll-y layout))}

      ;; Shift-click to extend the existing cursor range.
      (and shift-key? (not alt-key?) (not shortcut-key?) (= 1 click-count) (= 1 (count cursor-ranges)))
      (let [mouse-cursor (adjust-cursor lines (canvas->cursor layout lines x y))
            cursor-range (assoc (peek cursor-ranges) :to mouse-cursor)]
        {:cursor-ranges [cursor-range]
         :gesture-start (gesture-info :cursor-range-selection button click-count x y
                                      :reference-cursor-range cursor-range)})

      ;; Move cursor and prepare for box selection.
      (and alt-key? (not shift-key?) (= 1 click-count))
      (let [from-cursor (canvas->cursor layout lines x y)]
        {:cursor-ranges [(Cursor->CursorRange (adjust-cursor lines from-cursor))]
         :gesture-start (gesture-info :box-selection button click-count x y)})

      ;; Move cursor and prepare for drag-selection.
      (and (not alt-key?) (not shift-key?) (>= 3 ^long click-count))
      (let [mouse-cursor (adjust-cursor lines (canvas->cursor layout lines x y))
            cursor-range (case (long click-count)
                           1 (Cursor->CursorRange mouse-cursor)
                           2 (word-cursor-range-at-cursor lines mouse-cursor)
                           3 (->CursorRange (->Cursor (.row mouse-cursor) 0) (adjust-cursor lines (->Cursor (inc (.row mouse-cursor)) 0))))]
        {:cursor-ranges (if shortcut-key?
                          (concat-cursor-ranges cursor-ranges [cursor-range])
                          [cursor-range])
         :gesture-start (gesture-info :cursor-range-selection button click-count x y
                                      :reference-cursor-range cursor-range)}))))

(defn mouse-moved [lines cursor-ranges ^LayoutInfo layout ^GestureInfo gesture-start x y]
  (when (= :primary (some-> gesture-start :button))
    (case (.type gesture-start)
      ;; Dragging the scroll tab.
      :scroll-tab-y-drag
      (let [^double start-scroll (:scroll-y gesture-start)
            visible-ratio (visible-height-ratio layout (count lines))
            screen-delta (- ^double y ^double (:y gesture-start))
            scroll-delta (/ screen-delta visible-ratio)
            scroll-y (limit-scroll-y layout lines (- start-scroll scroll-delta))]
        (when (not= scroll-y (.scroll-y layout))
          {:scroll-y scroll-y}))

      ;; Box selection.
      :box-selection
      (let [^Rect canvas-rect (.canvas layout)
            range-inverted? (< ^double x (.x gesture-start))
            min-row (y->row layout (min ^double y (.y gesture-start)))
            max-row (y->row layout (max ^double y (.y gesture-start)))
            ^double min-x (if range-inverted? x (.x gesture-start))
            ^double max-x (if range-inverted? (.x gesture-start) x)
            line-min-x (- min-x (.x canvas-rect) (.scroll-x layout))
            box-cursor-ranges (into []
                                    (keep (fn [^long row]
                                            (when-some [line (get lines row)]
                                              (when (< line-min-x (line-width layout line))
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
                (not= cursor-ranges new-cursor-ranges) (merge {:cursor-ranges new-cursor-ranges})))

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
                                   :to (min-cursor (cursor-range-start word-cursor-range) (cursor-range-start reference-cursor-range)))
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
            new-cursor-ranges [cursor-range]]
        (when (not= cursor-ranges new-cursor-ranges)
          (merge {:cursor-ranges new-cursor-ranges}
                 (scroll-to-any-cursor layout lines new-cursor-ranges))))
      nil)))

(defn mouse-released []
  {:gesture-start nil})

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

(defn delete [lines cursor-ranges regions ^LayoutInfo layout delete-fn]
  (-> (if (every? cursor-range-empty? cursor-ranges)
        (splice lines regions (map (partial delete-fn lines) cursor-ranges))
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
            (assoc :scroll-y (limit-scroll-y layout lines (+ ^double scroll-y scroll-height))))))

(defn page-down [lines cursor-ranges scroll-y ^LayoutInfo layout move-type]
  (let [row-count (dec (.drawn-line-count layout))
        ^double line-height (line-height (.glyph layout))
        scroll-height (* row-count line-height)
        move-fn (move-type->move-fn move-type)
        cursor-fn (partial cursor-offset-down row-count)]
    (some-> (apply-move lines cursor-ranges layout move-fn cursor-fn)
            (assoc :scroll-y (limit-scroll-y layout lines (- ^double scroll-y scroll-height))))))

(defn select-next-occurrence [lines cursor-ranges ^LayoutInfo layout]
  ;; If there are one or more non-range cursors, we select the words under the cursors instead of the next occurrence.
  (if-some [replaced-cursor-ranges-by-index (not-empty (into []
                                                             (keep-indexed (fn [index cursor-range]
                                                                             (when (cursor-range-empty? cursor-range)
                                                                               (let [cursor-range' (word-cursor-range-at-cursor lines (CursorRange->Cursor cursor-range))]
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
            from-cursor (next-occurrence-search-cursor cursor-ranges)]
        (if-some [matching-cursor-range (find-next-occurrence lines needle-lines from-cursor false false)]
          (let [added-cursor-range (if (pos? (compare (.from needle-cursor-range) (.to needle-cursor-range)))
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

(defn indent [indent-level-pattern indent-string grammar lines cursor-ranges regions indent-string layout]
  (if (can-indent? lines cursor-ranges)
    (indent-lines lines cursor-ranges regions indent-string)
    (insert-text indent-level-pattern indent-string grammar lines cursor-ranges regions layout indent-string)))

(defn deindent [lines cursor-ranges regions tab-spaces]
  (deindent-lines lines cursor-ranges regions tab-spaces))

(defn select-and-frame [lines ^LayoutInfo layout cursor-range]
  (let [adjusted-cursor-range (adjust-cursor-range lines cursor-range)
        scroll-properties (scroll-to-cursor-range scroll-shortest scroll-center layout lines adjusted-cursor-range)]
    (assoc scroll-properties :cursor-ranges [adjusted-cursor-range])))

(defn find-next [lines cursor-ranges ^LayoutInfo layout needle-lines case-sensitive? whole-word? wrap?]
  (let [from-cursor (next-occurrence-search-cursor cursor-ranges)]
    (if-some [matching-cursor-range (find-next-occurrence lines needle-lines from-cursor case-sensitive? whole-word?)]
      (select-and-frame lines layout matching-cursor-range)
      (when (and wrap? (not= from-cursor document-start-cursor))
        (recur lines (with-next-occurrence-search-cursor cursor-ranges document-start-cursor) layout needle-lines case-sensitive? whole-word? wrap?)))))

(defn find-prev [lines cursor-ranges ^LayoutInfo layout needle-lines case-sensitive? whole-word? wrap?]
  (let [from-cursor (prev-occurrence-search-cursor cursor-ranges)]
    (if-some [matching-cursor-range (find-prev-occurrence lines needle-lines from-cursor case-sensitive? whole-word?)]
      (select-and-frame lines layout matching-cursor-range)
      (when wrap?
        (let [document-end-cursor (document-end-cursor lines)]
          (when (not= from-cursor document-end-cursor)
            (recur lines (with-prev-occurrence-search-cursor cursor-ranges document-end-cursor) layout needle-lines case-sensitive? whole-word? wrap?)))))))

(defn- replace-matching-selection [lines cursor-ranges regions needle-lines replacement-lines case-sensitive? whole-word?]
  (when (= 1 (count cursor-ranges))
    (let [cursor-range (adjust-cursor-range lines (first cursor-ranges))]
      (when (= cursor-range (find-next-occurrence lines needle-lines (cursor-range-start cursor-range) case-sensitive? whole-word?))
        (update (splice lines regions [[cursor-range replacement-lines]])
                :cursor-ranges (fn [[cursor-range' :as cursor-ranges']]
                                 (assert (= 1 (count cursor-ranges')))
                                 (-> cursor-ranges'
                                     (with-prev-occurrence-search-cursor (cursor-range-start cursor-range'))
                                     (with-next-occurrence-search-cursor (cursor-range-end cursor-range')))))))))

(defn replace-next [lines cursor-ranges regions ^LayoutInfo layout needle-lines replacement-lines case-sensitive? whole-word? wrap?]
  (if-some [splice-result (replace-matching-selection lines cursor-ranges regions needle-lines replacement-lines case-sensitive? whole-word?)]
    (merge splice-result (find-next (:lines splice-result) (:cursor-ranges splice-result) layout needle-lines case-sensitive? whole-word? wrap?))
    (find-next lines cursor-ranges layout needle-lines case-sensitive? whole-word? wrap?)))

(defn replace-all [lines regions needle-lines replacement-lines case-sensitive? whole-word?]
  (splice lines regions
          (loop [from-cursor document-start-cursor
                 splices (transient [])]
            (if-some [matching-cursor-range (find-next-occurrence lines needle-lines from-cursor case-sensitive? whole-word?)]
              (recur (cursor-range-end matching-cursor-range)
                     (conj! splices [matching-cursor-range replacement-lines]))
              (persistent! splices)))))
