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

(ns editor.diff-view
  (:require [clojure.string :as str]
            [editor.dialogs :as dialogs]
            [editor.localization :as localization]
            [editor.ui :as ui]
            [util.diff :as diff]
            [util.text-util :as text-util])
  (:import [javafx.scene Group Parent Scene]
           [javafx.scene.control Control ScrollBar]
           [javafx.scene.input KeyCode KeyEvent]
           [javafx.scene.layout Pane]
           [javafx.scene.shape Line Rectangle]
           [javafx.scene.text Font Text]))

(set! *warn-on-reflection* true)

(defn- make-text [font lines edit selector]
  (let [{:keys [begin end]} (selector edit)
        s (str/join "\n" (subvec lines begin end))
        t (Text. s)]
    (.setFont t font)
    (let [node (if (= begin end) (Rectangle. 10 0) t)]
      (ui/add-style! node (name (:type edit)))
      node)))

(defn- make-texts [^Group box lines edits selector]
  (let [font (Font. "Dejavu Sans Mono" 13)
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

(defn make-diff-viewer [left-name raw-str-left right-name raw-str-right localization]
  (let [root ^Parent (ui/load-fxml "diff.fxml")
        stage (doto (ui/make-stage)
                (.initOwner (ui/main-stage)))
        scene (Scene. root)
        str-left (text-util/crlf->lf raw-str-left)
        str-right (text-util/crlf->lf raw-str-right)
        {:keys [left-lines right-lines edits]} (diff/find-edits str-left str-right)]

    (ui/title! stage (localization (localization/message "dialog.diff-view.title")))
    (.setOnKeyPressed scene (ui/event-handler event (when (= (.getCode ^KeyEvent event) KeyCode/ESCAPE) (.close stage))))

    (.setScene stage scene)
    (ui/show! stage localization)

    (let [^Pane left (.lookup root "#left")
          ^Pane right (.lookup root "#right")
          ^Pane markers (.lookup root "#markers")
          left-group (Group.)
          right-group (Group.)
          ^ScrollBar hscroll (.lookup root "#hscroll")
          texts-left (make-texts left-group left-lines edits :left)
          texts-right (make-texts right-group right-lines edits :right)]

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

(defn present-diff-data [diff-data localization]
  (let [{:keys [binary? new new-path old old-path]} diff-data]
    (cond
      (= old new)
      (dialogs/make-info-dialog
        localization
        {:title (localization/message "dialog.diff-view.unchanged.title")
         :icon :icon/triangle-error
         :header (localization/message "dialog.diff-view.unchanged.header")})

      binary?
      (dialogs/make-info-dialog
        localization
        {:title (localization/message "dialog.diff-view.binary.title")
         :icon :icon/triangle-error
         :header (localization/message "dialog.diff-view.binary.header")})

      :else
      (make-diff-viewer old-path old new-path new localization))))
