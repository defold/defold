(ns editor.diff-view
  (:require [clojure.string :as str]
            [editor.dialogs :as dialogs]
            [editor.ui :as ui]
            [util.text-util :as text-util])
  (:import [javafx.scene Group Parent Scene]
           [javafx.scene.control Control ScrollBar]
           [javafx.scene.input KeyCode KeyEvent]
           [javafx.scene.layout Pane]
           [javafx.scene.shape Line Rectangle]
           [javafx.scene.text Font Text]
           [org.eclipse.jgit.diff Edit Edit$Type HistogramDiff RawTextComparator RawText]))

(set! *warn-on-reflection* true)

(defn- diff-strings [^String str-left ^String str-right]
  (.diff (HistogramDiff.) RawTextComparator/DEFAULT (RawText. (.getBytes str-left)) (RawText. (.getBytes str-right))))

(def edit-type->keyword {Edit$Type/REPLACE :replace Edit$Type/DELETE :delete Edit$Type/INSERT :insert Edit$Type/EMPTY :empty})

(defn- edit->map [^Edit edit]
  {:left {:begin (.getBeginA edit) :end (.getEndA edit)}
   :right {:begin (.getBeginB edit) :end (.getEndB edit)}
   :type (get edit-type->keyword (.getType edit))})

; TODO: Lot of logic to get "correct" number of lines
; NOTE: CRLF characters are expected to have been converted to LF in str
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

(defn- make-text [font lines edit selector]
  (let [{:keys [begin end]} (selector edit)
        s (str/join "\n" (subvec lines begin end))
        t (Text. s)]
    (.setFont t font)
    (let [node (if (= begin end) (Rectangle. 10 0) t)]
      (ui/add-style! node (name (:type edit)))
      node)))

(defn- make-texts [^Group box lines edits selector]
  (let [font (Font. "DejaVu Sans Mono" 13.0)
        texts (mapv (fn [edit] (make-text font lines edit selector)) edits)
        heights (reductions (fn [sum t] (+ sum (:height (ui/local-bounds t)))) 0 texts)]
    (doseq [[t y] (map vector texts heights)]
      (let [y' (+ (- (:miny (ui/local-bounds t))) y)]
        (if (instance? Text t)
          (.setY ^Text t y')
          (.setY ^Rectangle t y'))))
    (ui/children! box texts)
    texts))

(defn- make-boxes [^Pane box texts edits]
  (doseq [[text edit] (map vector texts edits)]
    (when-not (= (:type edit) :nop)
      (let [b (ui/local-bounds text)
            r (Rectangle. (:minx b) (:miny b) (:width b) (:height b))]
        (ui/add-styles! r ["diff-box" (name (:type edit))])
        (.bind (.widthProperty r) (.widthProperty box))
        (.add (.getChildren box) r)))))

(defn- make-line [^Text r1 ^Text r2 left right type]
  (let [b1 (ui/local-bounds r1)
        b2 (ui/local-bounds r2)]
    (let [line (Line. left (+ (:miny b1) (* 0.5 (:height b1))) right (+ (:miny b2) (* 0.5 (:height b2))))]
      (ui/add-style! line (name type))
      line)))

(defn- make-lines [^Pane box texts-left texts-right edits]
  (let [lines
        (for [[t-left t-right edit] (map vector texts-left texts-right edits)
              :when (not= (:type edit) :nop)]
          (make-line t-left t-right 0 40 (:type edit)))]
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

(defn make-diff-viewer [left-name raw-str-left right-name raw-str-right]
  (let [root ^Parent (ui/load-fxml "diff.fxml")
        stage (doto (ui/make-stage)
                (.initOwner (ui/main-stage)))
        scene (Scene. root)
        str-left (text-util/crlf->lf raw-str-left)
        str-right (text-util/crlf->lf raw-str-right)
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

      (make-boxes left texts-left edits)
      (make-boxes right texts-right edits)
      (make-lines markers texts-left texts-right edits)
      (.toFront left-group)
      (.toFront right-group))))

(defn present-diff-data [diff-data]
  (let [{:keys [binary? new new-path old old-path]} diff-data]
    (if (= old new)
      (dialogs/make-alert-dialog "The file is unchanged.")
      (if binary?
        (dialogs/make-alert-dialog "Unable to diff binary files.")
        (make-diff-viewer old-path old new-path new)))))

; TODO: Remove soon
#_(ui/run-later (make-diff-viewer "" "1\n2\n3\n4\n5\n6\n7\n8....\nAPA1\n...\n...\n...\n...\n...\n...\n...\n...\n...\n...\n"
                                  "" "XXX\n2\n3\n4\n6\n7\n8\nAPA1\n...\n...\n...\n...\n...\n...\n...\n...\n...\n...\nNEW STUFF\n"))
#_(ui/run-later (make-diff-viewer "a.txt" (slurp "a.txt") "b.txt" (slurp "b.txt")))
