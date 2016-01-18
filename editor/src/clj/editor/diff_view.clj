(ns editor.diff-view
  (:require [clojure.java.io :as io]
            [clojure.string :as str]
            [editor.ui :as ui])
  (:import [javafx.fxml FXMLLoader]
           [javafx.scene Parent Scene Group]
           [javafx.scene.control Control ScrollBar]
           [javafx.scene.input KeyCode KeyEvent]
           [javafx.scene.layout Pane]
           [javafx.scene.paint Paint]
           [javafx.scene.shape Line Rectangle]
           [javafx.scene.text Font Text]
           [javafx.stage Stage]
           [org.eclipse.jgit.diff Edit Edit$Type HistogramDiff RawTextComparator RawText]))

(set! *warn-on-reflection* true)

(defn- diff-strings [^String str-left ^String str-right]
  (.diff (HistogramDiff.) RawTextComparator/DEFAULT (RawText. (.getBytes str-left)) (RawText. (.getBytes str-right))))

(def edit-type->keyword {Edit$Type/REPLACE :replace Edit$Type/DELETE :delete Edit$Type/INSERT :insert Edit$Type/EMPTY :empty})

(defn- edit->map [^Edit edit]
  {:left {:begin (.getBeginA edit) :end (.getEndA edit)}
   :right {:begin (.getBeginB edit) :end (.getEndB edit)}
   :type (get edit-type->keyword (.getType edit))})

; TODO: \r and windows?
; TODO: Lot of logic to get "correct" number of lines
; NOTE: -1 is required to keep empty lines
(defn- split-lines [^String str]
  (let [lines (.split str "\n" -1)]
    (vec
      (if (.endsWith str "\n")
       (drop-last lines)
       lines))))

(defn- cut-edit [edit cutter]
  [(-> edit
     (assoc-in [:left :end]  (get-in cutter [:left :begin]))
     (assoc-in [:right :end] (get-in cutter [:right :begin])))
   cutter
   (-> edit
     (assoc-in [:left :begin] (get-in cutter [:left :end]))
     (assoc-in [:right :begin] (get-in cutter [:right :end])))])

(defn- redundant? [{left :left right :right type :type}]
  (and (= type :nop)
       (= (:begin left) (:end left))
       (= (:begin right) (:end right))))

(defn- insert-nop-edits [nop-edit edits]
  (let [[nop-edit' edits'] (->> edits (reduce (fn [[nop-edit' edits'] edit]
                                                (let [[a b c] (cut-edit nop-edit' edit)]
                                                  [c (conj edits' a b)]))
                                              [nop-edit []]))]
    (conj edits' nop-edit')))

(defn diff [^String str-left ^String str-right]
  (let [edits (map edit->map (diff-strings str-left str-right))
        lines-left (split-lines str-left)
        lines-right (split-lines str-right)
        nop-edit {:left {:begin 0 :end (count lines-left)}
                  :right {:begin 0 :end (count lines-right)}
                  :type :nop}
        edits' (insert-nop-edits nop-edit edits)]
    (remove redundant? edits')))

(defn- make-text [font lines {begin :begin end :end}]
  (let [s (str/join "\n" (subvec lines begin end))
        t (Text. s)]
    (.setFont t font)
    (if (= begin end) (Rectangle. 10 0) t)))

(defn- make-texts [^Group box lines edits selector]
  (let [font (Font. "Courier New" 13)
        texts (mapv (fn [e] (make-text font lines (selector e))) edits)
        heights (reductions (fn [sum t] (+ sum (:height (ui/local-bounds t)))) 0 texts)]
    (doseq [[^Text t y] (map vector texts heights)]
      (.setY t (+ (- (:miny (ui/local-bounds t))) y)))
    (ui/children! box texts)
    texts))

(defn- make-boxes [^Pane box texts edits selector]
  (doseq [[text edit] (map vector texts edits)]
    (when-not (= (:type edit) :nop)
      (let [b (ui/local-bounds text)
            r (Rectangle. (:minx b) (:miny b) (:width b) (:height b))]
        (ui/add-style! r "diff-box")
        (.bind (.widthProperty r) (.widthProperty box))
        (.add (.getChildren box) r)))))

(defn- make-line [^Text r1 ^Text r2 left right]
  (let [b1 (ui/local-bounds r1)
        b2 (ui/local-bounds r2)]
    (Line. left (+ (:miny b1) (* 0.5 (:height b1))) right (+ (:miny b2) (* 0.5 (:height b2))))))

(defn- make-lines [^Pane box texts-left texts-right edits]
  (let [lines
        (for [[t-left t-right edit] (map vector texts-left texts-right edits)
              :when (not= (:type edit) :nop)]
          (make-line t-left t-right 1 40))]
    (ui/children! box lines)))

(defn- clip-control! [^Control control]
  (let [clip (Rectangle.)]
        (.setClip control clip)
        (.bind (.widthProperty clip) (.widthProperty control))
        (.bind (.heightProperty clip) (.heightProperty control))))

(defn- update-scrollbar [^ScrollBar scroll ^Pane pane total-width]
  (let [w (:width (ui/local-bounds pane))
        m (- total-width w)]
    (.setMax scroll m)
    (.setVisibleAmount scroll (/ (* m w) total-width))))

(defn make-diff-viewer [left-name str-left right-name str-right]
  (let [root ^Parent (ui/load-fxml "diff.fxml")
        stage (Stage.)
        scene (Scene. root)
        lines-left (split-lines str-left)
        lines-right (split-lines str-right)
        edits (diff str-left str-right)]

    (ui/title! stage "Diff")
    (.setOnKeyPressed scene (ui/event-handler event (when (= (.getCode ^KeyEvent event) KeyCode/ESCAPE) (.close stage))))

    (.setScene stage scene)
    (ui/show! stage)

    (let [^Pane left (.lookup root "#left")
          ^Pane right (.lookup root "#right")
          ^Pane markers (.lookup root "#markers")
          left-group (Group.)
          right-group (Group.)
          ^ScrollBar hscroll (.lookup root "#hscroll")
          texts-left (make-texts left-group lines-left edits :left)
          texts-right (make-texts right-group lines-right edits :right)]

      (ui/text! (.lookup root "#left-file") left-name)
      (ui/text! (.lookup root "#right-file") right-name)

      (ui/children! left [left-group])
      (ui/children! right [right-group])

      (clip-control! left)
      (clip-control! right)

      (let [max-w #(reduce (fn [m t] (max m (:width (ui/local-bounds t)))) 0 %)
            total-width (max (max-w texts-left) (max-w texts-right))]

        (update-scrollbar hscroll left total-width)
        (ui/observe (.widthProperty left) (fn [_ old new] (update-scrollbar hscroll left total-width)))
        (ui/observe (.valueProperty hscroll) (fn [_ old new]
                                               (.setTranslateX left-group (- new))
                                               (.setTranslateX right-group (- new)))))

      (make-boxes left texts-left edits :left)
      (make-boxes right texts-right edits :right)
      (make-lines markers texts-left texts-right edits))))

; TODO: Remove soon
#_(ui/run-later (make-diff-viewer "" "1\n2\n3\n4\n5\n6\n7\n8....\nAPA1\n...\n...\n...\n...\n...\n...\n...\n...\n...\n...\n"
                                  "" "XXX\n2\n3\n4\n6\n7\n8\nAPA1\n...\n...\n...\n...\n...\n...\n...\n...\n...\n...\nNEW STUFF\n"))
#_(ui/run-later (make-diff-viewer "a.txt" (slurp "a.txt") "b.txt" (slurp "b.txt")))
