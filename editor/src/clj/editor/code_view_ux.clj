(ns editor.code-view-ux
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.dialogs :as dialogs]
            [editor.handler :as handler]
            [editor.ui :as ui])
  (:import  [javafx.scene.input KeyEvent MouseEvent]))

(defprotocol TextContainer
  (text! [this s])
  (text [this]))

(defprotocol TextScroller
  (preferred-offset [this])
  (preferred-offset! [this offset]))

(defprotocol TextView
  (text-selection [this])
  (text-selection! [this offset length])
  (selection-offset [this])
  (selection-length [this])
  (caret [this])
  (caret! [this offset select?]))

(defprotocol TextStyles
  (styles [this]))

(def mappings
  {:Right :right
   :Left :left
   :Shift+Right :select-right
   :Shift+Left :select-left
   :Up :up
   :Down :down
   :Shift+Up :select-up
   :Shift+Down :select-down
   :Control+Right :next-word
   :Alt+Right :next-word
   :Shift+Control+Right :select-next-word
   :Shift+Alt+Right :select-next-word
   :Control+Left :prev-word
   :Alt+Left :prev-word
   :Shift+Control+Left :select-prev-word
   :Shift+Alt+Left :select-prev-word
   :Meta+Left :line-begin
   :Control+A :line-begin
   :Shift+Meta+Left :select-line-begin
   :Shift+Control+A :select-line-begin
   :Meta+Right :line-end
   :Control+E :line-end
   :Shift+Meta+Right :select-line-end
   :Shift+Control+E :select-line-end
   :Meta+Up :file-begin
   :Meta+Down :file-end
   :Meta+L :goto-line
   :Double-Click :select-word
   })

(def tab-size 4)

(defn- info [e]
  {:event e
   :key-code (.getCode ^KeyEvent e)
   :control? (.isControlDown ^KeyEvent e)
   :meta? (.isMetaDown ^KeyEvent e)
   :alt? (.isAltDown ^KeyEvent e)
   :shift? (.isShiftDown ^KeyEvent e)})

(defn- add-modifier [info modifier-key modifier-str code-str]
  (if (get info modifier-key) (str modifier-str code-str) code-str))

;; potentially slow with each new keyword that is created
(defn- key-fn [info code]
  (let [code (->> (.getName code)
                 (add-modifier info :meta? "Meta+")
                 (add-modifier info :alt? "Alt+")
                 (add-modifier info :control? "Control+")
                 (add-modifier info :shift? "Shift+")
                 (keyword))]
    (println "code" code)
    (get mappings code)))

(defn- click-fn [click-count]
  (let [code (if (= click-count 2) :Double-Click :Single-Click)]
    (println "code" code)
    (get mappings code)))

(defn handle-key-pressed [e source-viewer]
  (let [k-info (info e)
        kf (key-fn k-info (.getCode ^KeyEvent e))]
    (when kf (handler/run
               kf
               [{:name :code-view :env {:selection source-viewer}}]
               k-info))))

(defn handle-mouse-clicked [e source-viewer]
  (println "Mouse clicked" e)
  (let [click-count (.getClickCount ^MouseEvent e)
        cf (click-fn click-count)]
    (when cf (handler/run
               cf
               [{:name :code-view :env {:selection source-viewer}}]
               e))))

(handler/defhandler :copy :code-view
  (enabled? [selection] selection)
  (run [selection clipboard]
    (text! clipboard (text-selection selection))))

(handler/defhandler :paste :code-view
  (enabled? [selection] selection)
  (run [selection clipboard]
    (when-let [clipboard-text (text clipboard)]
      (let [code (text selection)
            caret (caret selection)
            new-code (str (subs code 0 caret)
                          clipboard-text
                          (subs code caret (count code)))
            new-caret (+ caret (count clipboard-text))]
        (text! selection new-code)
        (caret! selection new-caret false)))))

(defn- adjust-bounds [s pos]
  (if (neg? pos) 0 (min (count s) pos)))

(defn right [selection select?]
  (let [c (caret selection)
          doc (text selection)
          next-pos (adjust-bounds doc (inc c))]
      (caret! selection next-pos select?)))

(defn left [selection select?]
  (let [c (caret selection)
        doc (text selection)
        next-pos (adjust-bounds doc (dec c))]
      (caret! selection next-pos select?)))

(handler/defhandler :right :code-view
  (enabled? [selection] selection)
  (run [selection]
    (right selection false)))

(handler/defhandler :left :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (left selection false)))

(handler/defhandler :select-right :code-view
  (enabled? [selection] selection)
  (run [selection]
    (right selection true)))

(handler/defhandler :select-left :code-view
  (enabled? [selection] selection)
  (run [selection]
    (left selection true)))

(defn tab-count [s]
  (let [tab-count (count (filter #(= \tab %) s))]
    tab-count))

(defn lines-before [s pos]
  (let [np (adjust-bounds s pos)]
    ;; an extra char is put in to pick up mult newlines
    (string/split (str (subs s 0 np) "x") #"\n")))

(defn lines-after [s pos]
  (let [np (adjust-bounds s pos)]
    (string/split (subs s np) #"\n")))

(defn up-line [s pos preferred-offset]
  (let [lines-before (lines-before s pos)
        len-before (-> lines-before last count dec)
        prev-line-tabs (-> lines-before drop-last last tab-count)
        prev-line-len (-> lines-before drop-last last count)
        preferred-next-pos (min preferred-offset (+ prev-line-len (* prev-line-tabs (dec tab-size))))
        actual-next-pos (- preferred-next-pos (* prev-line-tabs (dec tab-size)))
        adj-next-pos (+ (- pos len-before 1 prev-line-len)
                        (if (neg? actual-next-pos) 0 actual-next-pos))]
      (adjust-bounds s adj-next-pos)))

(defn down-line [s pos preferred-offset]
  (let [lines-after (lines-after s pos)
        len-after (-> lines-after first count)
        next-line-tabs (-> lines-after second tab-count)
        next-line-len (-> lines-after second count)
        preferred-next-pos (min preferred-offset (+ next-line-len (* next-line-tabs (dec tab-size))))
        actual-next-pos (- preferred-next-pos (* next-line-tabs (dec tab-size)))
        adj-next-pos (+ pos len-after 1 (if (neg? actual-next-pos) 0 actual-next-pos))]
      (adjust-bounds s adj-next-pos)))

(defn up [selection select?]
  (let [c (caret selection)
        doc (text selection)
        preferred-offset (preferred-offset selection)
        next-pos (up-line doc c preferred-offset)]
    (caret! selection next-pos select?)))

(defn down [selection select?]
  (let [c (caret selection)
        doc (text selection)
        preferred-offset (preferred-offset selection)
        next-pos (down-line doc c preferred-offset)]
      (caret! selection next-pos select?)))

(handler/defhandler :up :code-view
  (enabled? [selection] selection)
  (run [selection]
    (up selection false)))

(handler/defhandler :down :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (down selection false)))

(handler/defhandler :select-up :code-view
  (enabled? [selection] selection)
  (run [selection]
    (up selection true)))

(handler/defhandler :select-down :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (down selection true)))

(def word-regex  #"\n|\w+|\s\w+|.")

(defn next-word [selection select?]
  (let [c (caret selection)
        doc (text selection)
        np (adjust-bounds doc c)
        next-word-move (re-find word-regex (subs doc np))
        next-pos (+ np (count next-word-move))]
    (caret! selection next-pos select?)))

(defn prev-word [selection select?]
  (let [c (caret selection)
        doc (text selection)
        np (adjust-bounds doc c)
        next-word-move (re-find word-regex (->> (subs doc 0 np) (reverse) (apply str)))
        next-pos (- np (count next-word-move))]
    (caret! selection next-pos select?)))

(handler/defhandler :next-word :code-view
  (enabled? [selection] selection)
  (run [selection]
    (next-word selection false)))

(handler/defhandler :prev-word :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (prev-word selection false)))

(handler/defhandler :select-next-word :code-view
  (enabled? [selection] selection)
  (run [selection]
    (next-word selection true)))

(handler/defhandler :select-prev-word :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (prev-word selection true)))

(defn line-begin [selection select?]
  (let [c (caret selection)
        doc (text selection)
        np (adjust-bounds doc c)
        lines-before (lines-before doc np)
        len-before (-> lines-before last count dec)
        next-pos (- np len-before)]
    (caret! selection next-pos select?)))

(defn line-end [selection select?]
  (let [c (caret selection)
        doc (text selection)
        np (adjust-bounds doc c)
        lines-after (lines-after doc np)
        len-after (-> lines-after first count)
        next-pos (+ np len-after)]
    (caret! selection next-pos select?)))

(handler/defhandler :line-begin :code-view
  (enabled? [selection] selection)
  (run [selection]
    (line-begin selection false)))

(handler/defhandler :line-end :code-view
  (enabled? [selection] selection)
  (run [selection]
    (line-end selection false)))

(handler/defhandler :select-line-begin :code-view
  (enabled? [selection] selection)
  (run [selection]
    (line-begin selection true)))

(handler/defhandler :select-line-end :code-view
  (enabled? [selection] selection)
  (run [selection]
    (line-end selection true)))

(handler/defhandler :file-begin :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (caret! selection 0 false)))

(handler/defhandler :file-end :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (let [doc (text selection)]
      (caret! selection (count doc) false))))

(defn go-to-line [selection line-number]
  (try
    (let [line  (Integer/parseInt line-number)
          doc (text selection)
          doc-lines (string/split doc #"\n")
          target-lines (take (dec line) doc-lines)
          np (+ (count target-lines) (reduce + (map count target-lines)))]
      (caret! selection (adjust-bounds doc (if (zero? np) 0 np)) false))
    (catch Exception e (println "Not a valid line number" line-number (.getMessage e)))))

(handler/defhandler :goto-line :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    ;; when using show and wait on the dialog, was getting very
    ;; strange double events not solved by consume - this is a
    ;; workaround to let us use show
    (let [line-number (promise)
          stage (dialogs/make-goto-line-dialog line-number)
          go-to-line-fn (fn [] (when (realized? line-number)
                                (go-to-line selection @line-number)))]
      (.setOnHidden stage (ui/event-handler e (go-to-line-fn))))))

(handler/defhandler :select-word :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (let [regex #"^\w+"
          c (caret selection)
          doc (text selection)
          np (adjust-bounds doc c)
          word-end (re-find regex (subs doc np))
          text-before (->> (subs doc 0 (inc np)) (reverse) (rest) (apply str))
          word-begin (re-find regex text-before)
          start-pos (- np (count word-begin))
          end-pos (+ np (count word-end))
          len (- end-pos start-pos)]
      (text-selection! selection start-pos len))))
