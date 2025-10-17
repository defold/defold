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

(ns editor.console
  (:require [cljfx.api :as fx]
            [cljfx.fx.button :as fx.button]
            [cljfx.fx.check-box :as fx.check-box]
            [cljfx.fx.h-box :as fx.h-box]
            [cljfx.fx.label :as fx.label]
            [cljfx.fx.list-cell :as fx.list-cell]
            [cljfx.fx.list-view :as fx.list-view]
            [cljfx.fx.popup :as fx.popup]
            [cljfx.fx.region :as fx.region]
            [cljfx.fx.separator :as fx.separator]
            [cljfx.fx.stack-pane :as fx.stack-pane]
            [cljfx.fx.text-field :as fx.text-field]
            [cljfx.fx.v-box :as fx.v-box]
            [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code.data :as data]
            [editor.code.resource :as r]
            [editor.code.util :as util :refer [re-match-result-seq split-lines]]
            [editor.code.view :as view]
            [editor.error-reporting :as error-reporting]
            [editor.fxui :as fxui]
            [editor.graph-util :as gu]
            [editor.handler :as handler]
            [editor.localization :as localization]
            [editor.prefs :as prefs]
            [editor.resource :as resource]
            [editor.types :as types]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [util.coll :as coll]
            [util.http-server :as http-server])
  (:import [editor.code.data Cursor CursorRange LayoutInfo Rect]
           [java.io BufferedReader IOException]
           [java.util.regex MatchResult]
           [javafx.beans.property SimpleStringProperty]
           [javafx.scene Node Parent Scene]
           [javafx.scene.canvas Canvas GraphicsContext]
           [javafx.scene.control Button Tab TextField]
           [javafx.scene.input Clipboard KeyEvent MouseEvent ScrollEvent]
           [javafx.scene.layout GridPane Pane]
           [javafx.scene.paint Color]
           [javafx.scene.text Font FontSmoothingType TextAlignment]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce ^:const url-prefix "/console")

(def ^:const console-filters-prefs-key [:console :filters])
(def ^:const console-filtering-key [:console :filtering])

(def ^:private pending-atom
  ;; Implementation notes:
  ;; For filtering, we have to keep all received output entries. Each frame,
  ;; the console view consumes a batch of entries — this is implemented by
  ;; incrementing the consumption :index.
  ;; Filtering is implemented by changing :filter-set (and updating :entry-pred,
  ;; which is derived from :filter-set and stored for performance), and then
  ;; by resetting the :index to 0, marking the view to :clear while keeping the
  ;; :entries. Hence, from the point of view of the console view, there is no
  ;; such thing as filters, only clearing the output.
  (atom {:clear false :entries [] :index 0 :filter-set #{} :entry-pred nil}))

(def ^:private gutter-bubble-font
  (Font. "Source Sans Pro", 10.5))

(def ^:private gutter-bubble-glyph-metrics
  (view/make-glyph-metrics gutter-bubble-font 1.0))

(defn append-console-entry!
  "Append to the console. Callable from a background thread. If type is
  non-nil, a region of the specified type will encompass the line."
  [type line]
  (assert (or (nil? type) (keyword? type)))
  (assert (string? line))
  (swap! pending-atom update :entries conj [type line]))

(def append-console-line! (partial append-console-entry! nil))

(defn clear-console!
  "Clear the console. Callable from a background thread."
  []
  (swap! pending-atom assoc :clear true :entries [] :index 0))

(def ^:private remote-log-pump-thread (atom nil))
(def ^:private console-stream (atom nil))

(defn current-stream? [stream] (= @console-stream stream))

(defn reset-remote-log-pump-thread! [^Thread new]
  (when-let [old ^Thread @remote-log-pump-thread]
    (.interrupt old))
  (reset! remote-log-pump-thread new))

(defn reset-console-stream! [stream]
  (reset! console-stream stream)
  (clear-console!))

;; Start a background thread that continuously reads lines from log-stream and calls sink-fn for each line.
(defn start-log-pump! [log-stream sink-fn]
  (doto (Thread. (fn []
                   (try
                     (let [this (Thread/currentThread)]
                       (with-open [buffered-reader ^BufferedReader (io/reader log-stream :encoding "UTF-8")]
                         (loop []
                           (when-not (.isInterrupted this)
                             (when-let [line (.readLine buffered-reader)] ; line of text or nil if eof reached
                               (sink-fn line)
                               (recur))))))
                     (catch IOException _
                       ;; Losing the log connection is ok and even expected
                       nil)
                     (catch InterruptedException _
                       ;; Losing the log connection is ok and even expected
                       nil))))
    (.start)))

(defn make-remote-log-sink [log-stream]
  (fn [line]
    (when (= @console-stream log-stream)
      (append-console-line! line))))

(defn set-log-service-stream [log-stream]
  (reset-console-stream! log-stream)
  (reset-remote-log-pump-thread! (start-log-pump! log-stream (make-remote-log-sink log-stream))))

(defn pipe-log-stream-to-console! [input-stream]
  (reset-console-stream! input-stream)
  (reset-remote-log-pump-thread! (start-log-pump! input-stream (make-remote-log-sink input-stream))))

(defn- consume-entries-in-state [state ^long n]
  (let [{:keys [^long index entries]} state]
    (assoc state :index (min (count entries) (+ index n)) :clear false)))

(defn- get-entries-from-state [state ^long n]
  (let [{:keys [^long index entries entry-pred]} state
        unfiltered (subvec entries index (min (count entries) (+ index n)))]
    (if entry-pred
      (filterv entry-pred unfiltered)
      unfiltered)))

(defn- dequeue-pending! [n]
  (let [old-state (first (swap-vals! pending-atom consume-entries-in-state n))]
    (assoc old-state :entries (get-entries-from-state old-state n))))

(defn- compile-entry-predicate [filter-set]
  (letfn [(make-inclusion-pred [inclusion]
            (fn inclusion-pred [entry]
              (string/includes? (entry 1) inclusion)))
          (make-exclusion-pred [exclusion-with-prefix]
            (let [exclusion (subs exclusion-with-prefix 1)]
              (fn exclusion-pred [entry]
                (not (string/includes? (entry 1) exclusion)))))
          (combine-preds [pred-combinator preds]
            (case (count preds)
              0 nil
              1 (first preds)
              (apply pred-combinator preds)))]
    (let [[exclusions inclusions] (coll/separate-by #(and (<= 2 (count %))
                                                          (string/starts-with? % "!"))
                                                    filter-set)
          inclusions-pred (combine-preds some-fn (mapv make-inclusion-pred inclusions))
          exclusions-pred (combine-preds every-pred (mapv make-exclusion-pred exclusions))]
      (combine-preds every-pred (filterv some? [inclusions-pred exclusions-pred])))))

(defn- set-filters! [filters]
  (let [filter-set (into #{}
                         (keep (fn [[text enabled]]
                                 (when enabled text)))
                         filters)]
    (swap! pending-atom (fn [state]
                          (if (= filter-set (:filter-set state))
                            state
                            (assoc state
                              :filter-set filter-set
                              :entry-pred (compile-entry-predicate filter-set)
                              :clear true
                              :index 0))))))

(defn- save-filters! [prefs filters]
  (let [filtering (prefs/get prefs console-filtering-key)]
    (prefs/set! prefs console-filters-prefs-key filters)
    (set-filters! (if filtering filters []))))

;; -----------------------------------------------------------------------------
;; Tool Bar
;; -----------------------------------------------------------------------------
(def ^:private ext-with-button-props
  (fx/make-ext-with-props fx.button/props))

(defn filter-console-list-cell-view [[i [text selected] :as in]]
  (if-not in
    {}
    {:style-class "console-filter-popup-list-cell"
     ;; somehow this line makes the check-box's label to limit its width, so it
     ;; does not produce a horizontal scrollbar, but instead truncates the text
     ;; with ellipsis.
     :pref-width 100
     :graphic {:fx/type fx.h-box/lifecycle
               :style-class "console-filter-popup-list-cell-h-box"
               :alignment :center-left
               :spacing 2
               :children [{:fx/type fx.check-box/lifecycle
                           :h-box/hgrow :always
                           :focus-traversable false
                           :max-width ##Inf
                           :selected selected
                           :mnemonic-parsing false
                           :on-selected-changed {:event-type :select :index i}
                           :text text}
                          {:fx/type fx.button/lifecycle
                           :focus-traversable false
                           :style-class "console-filter-popup-list-cell-remove-button"
                           :graphic {:fx/type fx.region/lifecycle
                                     :style-class "cross"}
                           :on-action {:event-type :delete :index i}}]}}))

(defn- filter-console-view [^Node filter-console-button localization {:keys [open enabled filters text]}]
  (let [active-filters-count (count (filterv second filters))
        show-counter (and enabled (pos? active-filters-count))
        anchor (.localToScreen filter-console-button
                               -12.0 ;; shadow offset
                               (- (.getMaxY (.getBoundsInLocal filter-console-button))
                                  ;; shadow offset
                                  4.0))]
    {:fx/type fxui/with-popup-window
     :desc {:fx/type ext-with-button-props
            :desc {:fx/type fxui/ext-value
                   :value filter-console-button}
            :props {:on-action {:event-type :show-or-hide}
                    :graphic {:fx/type fx.h-box/lifecycle
                              :style-class "content"
                              :children [{:fx/type fx.label/lifecycle
                                          :visible show-counter
                                          :managed show-counter
                                          :id "filter-console-counter"
                                          :text (str active-filters-count)}
                                         {:fx/type fx.region/lifecycle
                                          :pseudo-classes (if open #{:open} #{})
                                          :h-box/margin {:left 10}
                                          :id "filter-console-arrow"}]}}}
     :popup {:fx/type fx.popup/lifecycle
             :showing open
             :anchor-location :window-bottom-left
             :anchor-x (.getX anchor)
             :anchor-y (.getY anchor)
             :auto-hide true
             :auto-fix true
             :hide-on-escape true
             :consume-auto-hiding-events true
             :on-auto-hide {:event-type :hide}
             :content [{:fx/type fx.stack-pane/lifecycle
                        :stylesheets [(str (io/resource "editor.css"))]
                        :style-class "console-filter-popup"
                        :children [{:fx/type fx.region/lifecycle
                                    :mouse-transparent true
                                    :style-class "console-filter-popup-background"}
                                   {:fx/type fx.v-box/lifecycle
                                    :children
                                    [{:fx/type fxui/ext-localize
                                      :v-box/margin 4
                                      :localization localization
                                      :message (localization/message "console.filter.enable-filtering")
                                      :desc {:fx/type fx.check-box/lifecycle
                                             :focus-traversable false
                                             :max-width ##Inf
                                             :selected enabled
                                             :on-selected-changed {:event-type :toggle-global-filtering}}}
                                     {:fx/type fx.separator/lifecycle
                                      :style-class "console-filter-popup-separator"}
                                     {:fx/type fx.list-view/lifecycle
                                      :focus-traversable false
                                      :style-class "console-filter-popup-list-view"
                                      :items (into [] (map-indexed coll/pair) filters)
                                      :fixed-cell-size 27
                                      :max-height (* 27 (min 10 (count filters)))
                                      :cell-factory {:fx/cell-type fx.list-cell/lifecycle
                                                     :describe filter-console-list-cell-view}}
                                     {:fx/type fxui/ext-focused-by-default
                                      :v-box/margin 4
                                      :desc {:fx/type fxui/ext-localize
                                             :localization localization
                                             :message (localization/message "console.filter.add-filter")
                                             :object-fn TextField/.promptTextProperty
                                             :desc {:fx/type fx.text-field/lifecycle
                                                    :text text
                                                    :on-text-changed {:event-type :type}
                                                    :on-action {:event-type :add}}}}]}]}]}}))

(defn- handle-filter-event! [state prefs e]
  (case (:event-type e)
    :hide (swap! state assoc :open false)
    :show-or-hide (swap! state update :open not)
    :toggle-global-filtering (let [enabled (not (:enabled @state))]
                               (prefs/set! prefs console-filtering-key enabled)
                               (set-filters! (if enabled (:filters @state) []))
                               (swap! state assoc :enabled enabled))
    :type (swap! state assoc :text (:fx/event e))
    :delete (let [new-state (swap! state update :filters util/remove-index (:index e))]
              (save-filters! prefs (:filters new-state)))
    :add (let [new-state (swap! state #(-> %
                                           (assoc :text "")
                                           (cond-> (not (string/blank? (:text %)))
                                                   (update :filters conj [(:text %) true]))))]
           (save-filters! prefs (:filters new-state)))
    :select (let [new-state (swap! state assoc-in [:filters (:index e) 1] (:fx/event e))]
              (save-filters! prefs (:filters new-state)))))

(defn- init-console-filter! [filter-console-button prefs localization]
  (let [filters (prefs/get prefs console-filters-prefs-key)
        filtering (prefs/get prefs console-filtering-key)
        state (atom {:open false :enabled filtering :text "" :filters filters})]
    (set-filters! (if filtering filters []))
    (fx/mount-renderer
      state
      (fx/create-renderer
        :error-handler error-reporting/report-exception!
        :opts {:fx.opt/map-event-handler #(handle-filter-event! state prefs %)}
        :middleware (comp
                      fxui/wrap-dedupe-desc
                      (fx/wrap-map-desc #(filter-console-view filter-console-button localization %)))))))

(defonce ^SimpleStringProperty find-term-property (doto (SimpleStringProperty.) (.setValue "")))

(defn- setup-tool-bar!
  ^Parent [^Parent tool-bar view-node prefs localization]
  (ui/with-controls tool-bar [^TextField search-console
                              ^Button prev-console
                              ^Button next-console
                              ^Button clear-console
                              filter-console]
    (localization/localize! clear-console localization (localization/message "console.button.clear"))
    (localization/localize! filter-console localization (localization/message "console.button.filter"))
    (localization/localize! (.promptTextProperty search-console) localization (localization/message "console.search"))
    (init-console-filter! filter-console prefs localization)
    (ui/context! tool-bar :console-tool-bar {:term-field search-console :view-node view-node} nil)
    (.bindBidirectional (.textProperty search-console) find-term-property)
    (ui/bind-key-commands! search-console {"Enter" :code.find-next
                                           "Shift+Enter" :code.find-previous})
    (ui/bind-action! prev-console :code.find-previous)
    (ui/bind-action! next-console :code.find-next)
    (ui/bind-action! clear-console :console.clear))
  tool-bar)

(defn- dispose-tool-bar! [^Parent tool-bar]
  (ui/with-controls tool-bar [^TextField search-console]
    (.unbindBidirectional (.textProperty search-console) find-term-property)))

(defn- focus-term-field! [^Parent bar]
  (ui/with-controls bar [^TextField search-console]
    (.requestFocus search-console)
    (.selectAll search-console)))

(defn- set-find-term! [^String term-text]
  (.setValue find-term-property (or term-text "")))

(defn- find-next! [view-node]
  (view/set-properties! view-node :selection
                        (data/find-next (view/get-property view-node :lines)
                                        (view/get-property view-node :cursor-ranges)
                                        (view/get-property view-node :layout)
                                        (split-lines (.getValue find-term-property))
                                        false
                                        false
                                        true)))

(defn- find-prev! [view-node]
  (view/set-properties! view-node :selection
                        (data/find-prev (view/get-property view-node :lines)
                                        (view/get-property view-node :cursor-ranges)
                                        (view/get-property view-node :layout)
                                        (split-lines (.getValue find-term-property))
                                        false
                                        false
                                        true)))

(handler/defhandler :edit.find :console-view
  (run [term-field view-node]
       (when-some [selected-text (view/non-empty-single-selection-text view-node)]
         (set-find-term! selected-text))
       (focus-term-field! term-field)))

(handler/defhandler :code.find-next :console-view
  (run [view-node] (find-next! view-node)))

(handler/defhandler :code.find-next :console-tool-bar
  (run [view-node] (find-next! view-node)))

(handler/defhandler :code.find-previous :console-view
  (run [view-node] (find-prev! view-node)))

(handler/defhandler :code.find-previous :console-tool-bar
  (run [view-node] (find-prev! view-node)))

;; -----------------------------------------------------------------------------
;; Read-only code view action handlers
;; -----------------------------------------------------------------------------

(view/register-fundamental-read-only-handlers! *ns* :console-view :console-tool-bar)

;; -----------------------------------------------------------------------------
;; Console view action handlers
;; -----------------------------------------------------------------------------

(handler/defhandler :console.clear :console-tool-bar
  (run [view-node] (clear-console!)))

;; -----------------------------------------------------------------------------
;; Setup
;; -----------------------------------------------------------------------------

(defmulti json-compatible-region (fn [region] (:type region)))

(defmethod json-compatible-region :default [region] (dissoc region :on-click!))

(g/defnk produce-request-response [lines regions]
  (http-server/json-response {:lines lines :regions (into [] (keep json-compatible-region) regions)}))

(g/defnode ConsoleNode
  (property indent-type r/IndentType (default :two-spaces))
  (property cursor-ranges r/CursorRanges (default [data/document-start-cursor-range]) (dynamic visible (g/constantly false)))
  (property invalidated-rows r/InvalidatedRows (default []) (dynamic visible (g/constantly false)))
  (property modified-lines types/Lines (default [""]) (dynamic visible (g/constantly false)))
  (property regions r/Regions (default []) (dynamic visible (g/constantly false)))
  (output lines types/Lines (gu/passthrough modified-lines))
  (output request-response g/Any :cached produce-request-response))

(defn- gutter-metrics []
  [44.0 0.0])

(defn- draw-gutter! [^GraphicsContext gc ^Rect gutter-rect ^LayoutInfo layout ^Font font color-scheme lines regions]
  (let [glyph-metrics (.glyph layout)
        ^double line-height (data/line-height glyph-metrics)
        ^double ascent (data/ascent glyph-metrics)
        ^double gutter-bubble-ascent (data/ascent gutter-bubble-glyph-metrics)
        visible-regions (data/visible-cursor-ranges lines layout regions)
        text-right (Math/floor (- (+ (.x gutter-rect) (.w gutter-rect)) (/ line-height 2.0) 3.0))
        bubble-background-color (Color/valueOf "rgba(255, 255, 255, 0.1)")
        ^Color gutter-foreground-color (view/color-lookup color-scheme "editor.gutter.foreground")
        gutter-background-color (view/color-lookup color-scheme "editor.gutter.background")
        gutter-shadow-color (view/color-lookup color-scheme "editor.gutter.shadow")
        gutter-eval-expression-color (view/color-lookup color-scheme "editor.gutter.eval.expression")
        gutter-eval-error-color (view/color-lookup color-scheme "editor.gutter.eval.error")
        gutter-eval-result-color (view/color-lookup color-scheme "editor.gutter.eval.result")]

    ;; Draw gutter background and shadow when scrolled horizontally.
    (when (neg? (.scroll-x layout))
      (.setFill gc gutter-background-color)
      (.fillRect gc (.x gutter-rect) (.y gutter-rect) (.w gutter-rect) (.h gutter-rect))
      (.setFill gc gutter-shadow-color)
      (.fillRect gc (+ (.x gutter-rect) (.w gutter-rect)) 0.0 8.0 (.h gutter-rect)))

    ;; Draw gutter annotations.
    (.setFontSmoothingType gc FontSmoothingType/LCD)
    (.setTextAlign gc TextAlignment/RIGHT)
    (doseq [^CursorRange region visible-regions]
      (let [line-y (data/row->y layout (.row ^Cursor (.from region)))]
        (case (:type region)
          :repeat
          (let [^long repeat-count (:count region)
                overflow? (<= 1000 repeat-count)
                repeat-text (if overflow?
                              (format "+9%02d" (mod repeat-count 100))
                              (str repeat-count))
                text-top (+ 2.0 line-y)
                text-bottom (+ gutter-bubble-ascent text-top)
                text-width (Math/ceil (data/text-width gutter-bubble-glyph-metrics repeat-text))
                ^double text-height (data/line-height gutter-bubble-glyph-metrics)
                text-left (- text-right text-width)
                ^Rect bubble-rect (data/expand-rect (data/->Rect text-left text-top text-width text-height) 3.0 0.0)]
            (.setFill gc bubble-background-color)
            (.fillRoundRect gc (.x bubble-rect) (.y bubble-rect) (.w bubble-rect) (.h bubble-rect) 5.0 5.0)
            (.setFont gc gutter-bubble-font)
            (.setFill gc gutter-foreground-color)
            (.fillText gc repeat-text text-right text-bottom))

          :eval-expression
          (let [text-y (+ ascent line-y)]
            (.setFont gc font)
            (.setFill gc gutter-eval-expression-color)
            (.fillText gc ">" text-right text-y))

          :eval-result
          (let [text-y (+ ascent line-y)]
            (.setFont gc font)
            (.setFill gc gutter-eval-result-color)
            (.fillText gc "=" text-right text-y))

          :eval-error
          (let [text-y (+ ascent line-y)]
            (.setFont gc font)
            (.setFill gc gutter-eval-error-color)
            (.fillText gc "!" text-right text-y))

          :extension-output
          (let [text-y (+ ascent line-y)]
            (.setFont gc font)
            (.setFill gc gutter-eval-expression-color)
            (.fillText gc "⚙" text-right text-y))

          :extension-error
          (let [text-y (+ ascent line-y)]
            (.setFont gc font)
            (.setFill gc gutter-eval-error-color)
            (.fillText gc "⚙" text-right text-y))
          nil)))))

(deftype ConsoleGutterView []
  view/GutterView

  (gutter-metrics [_this _lines _regions _glyph-metrics]
    (gutter-metrics))

  (draw-gutter! [_this gc gutter-rect layout font color-scheme lines regions _visible-cursors]
    (draw-gutter! gc gutter-rect layout font color-scheme lines regions)))

(defn- setup-view! [console-node view-node]
  (g/transact
    (concat
      (g/connect console-node :_node-id view-node :resource-node)
      (g/connect console-node :indent-type view-node :indent-type)
      (g/connect console-node :cursor-ranges view-node :cursor-ranges)
      (g/connect console-node :invalidated-rows view-node :invalidated-rows)
      (g/connect console-node :lines view-node :lines)
      (g/connect console-node :regions view-node :regions)))
  view-node)

(def ^:const line-sub-regions-pattern #"(?<=^|\s|[<\"'`])(\/[^\s>\"'`:]+)(?::?)(\d+)?")
(def ^:private ^:const line-sub-regions-pattern-partial #"([^\s<>:]+):(\d+)")

(def ^:private non-empty-string? (comp string? not-empty))

(defn- make-resource-reference-region
  ([row start-col end-col resource-proj-path-candidates on-click!]
   {:pre [(vector? resource-proj-path-candidates)
          (not-empty resource-proj-path-candidates)
          (every? non-empty-string? resource-proj-path-candidates)
          (ifn? on-click!)]}
   (assoc (data/->CursorRange (data/->Cursor row start-col)
                              (data/->Cursor row end-col))
     :type :resource-reference
     :proj-path-candidates resource-proj-path-candidates
     :on-click! on-click!))
  ([row start-col end-col resource-proj-path-candidates resource-row on-click!]
   {:pre [(integer? resource-row)
          (not (neg? ^long resource-row))]}
   (assoc (make-resource-reference-region row start-col end-col resource-proj-path-candidates on-click!)
     :row resource-row)))

(definline resource-path-suffix-key [^String resource-path]
  `(.substring ~resource-path (max 0 (- (.length ~resource-path) 50))))

(defn- find-project-resource-paths-from-potential-match [resource-map resource-suffix-map-delay partial-path]
  (cond
    (string/starts-with? partial-path "/")
    (when (contains? resource-map partial-path)
      [partial-path])

    (string/starts-with? partial-path "...")
    (let [suffix (subs partial-path 3)]
      (->> (get @resource-suffix-map-delay (resource-path-suffix-key suffix))
           (filter #(string/ends-with? % suffix))
           sort
           vec))

    :else
    (let [with-slash (str "/" partial-path)]
      (when (contains? resource-map with-slash)
        [with-slash]))))

(defn make-resource-suffix-map-delay [resource-map]
  (delay (group-by resource-path-suffix-key (keys resource-map))))

(defn- make-line-sub-regions [resource-map resource-suffix-map-delay on-region-click! row line]
  (into []
        (comp
          (mapcat #(re-match-result-seq % line))
          (keep (fn [^MatchResult result]
                  (when-some [resource-proj-path-candidates (not-empty (find-project-resource-paths-from-potential-match resource-map resource-suffix-map-delay (.group result 1)))]
                    (let [resource-row (some-> (.group result 2) Long/parseUnsignedLong)
                          start-col (.start result)
                          end-col (if (string/ends-with? (.group result) ":")
                                    (dec (.end result))
                                    (.end result))]
                      (if (or (nil? resource-row)
                              (neg? (dec (long resource-row))))
                        (make-resource-reference-region row start-col end-col resource-proj-path-candidates on-region-click!)
                        (make-resource-reference-region row start-col end-col resource-proj-path-candidates (dec (long resource-row)) on-region-click!))))))
          (distinct))
        [line-sub-regions-pattern line-sub-regions-pattern-partial]))

(defn- make-whole-line-region [type ^long row line]
  (assert (keyword? type))
  (assert (not (neg? row)))
  (assert (string? line))
  (assoc (data/->CursorRange (data/->Cursor row 0)
                             (data/->Cursor row (count line)))
    :type type))

(defn- make-line-regions [resource-map resource-suffix-map-delay on-region-click! row [type line]]
  (assert (keyword? type))
  (assert (string? line))
  (cons (make-whole-line-region type row line)
        (make-line-sub-regions resource-map resource-suffix-map-delay on-region-click! row line)))

(defn- append-distinct-lines [{:keys [lines regions] :as props} entries resource-map resource-suffix-map-delay on-region-click!]
  (merge props
         (data/append-distinct-lines lines regions
                                     (mapv second entries)
                                     (partial make-line-sub-regions resource-map resource-suffix-map-delay on-region-click!))))

(defn- append-regioned-lines [{:keys [lines regions] :as props} entries resource-map resource-suffix-map-delay on-region-click!]
  (assert (vector? lines))
  (assert (vector? regions))
  (let [clean-lines (if (= [""] lines) [] lines)
        lines' (into clean-lines (map second) entries)
        lines' (if (empty? lines') [""] lines')
        regions' (into regions
                       (mapcat (partial make-line-regions resource-map resource-suffix-map-delay on-region-click!)
                               (iterate inc (count clean-lines))
                               entries))]
    (cond-> (assoc props
              :lines lines'
              :regions regions')

            (empty? clean-lines)
            (assoc :invalidated-row 0))))

(defn- append-entries [props entries resource-map resource-suffix-map-delay on-region-click!]
  (assert (map? props))
  (assert (vector? (not-empty (:lines props))))
  (assert (vector? (:regions props)))
  (reduce (fn [props entries]
            (if (nil? (ffirst entries))
              (append-distinct-lines props entries resource-map resource-suffix-map-delay on-region-click!)
              (append-regioned-lines props entries resource-map resource-suffix-map-delay on-region-click!)))
          props
          (partition-by #(nil? (first %)) entries)))

(defn- repaint-console-view! [view-node workspace on-region-click! elapsed-time]
  (let [{:keys [clear entries]} (dequeue-pending! 1024)]
    (when (or clear (seq entries))
      (g/with-auto-evaluation-context evaluation-context
        (let [resource-map (g/node-value workspace :resource-map evaluation-context)
              ^LayoutInfo prev-layout (g/node-value view-node :layout evaluation-context)
              prev-lines (g/node-value view-node :lines evaluation-context)
              prev-regions (g/node-value view-node :regions evaluation-context)
              prev-document-width (if clear 0.0 (.document-width prev-layout))
              appended-width (data/max-line-width (.glyph prev-layout) (.tab-stops prev-layout) (mapv second entries))
              document-width (max prev-document-width ^double appended-width)
              was-scrolled-to-bottom? (data/scrolled-to-bottom? prev-layout (count prev-lines))
              resource-suffix-map-delay (make-resource-suffix-map-delay resource-map)
              props (append-entries {:lines (if clear [""] prev-lines)
                                     :regions (if clear [] prev-regions)}
                                    entries resource-map resource-suffix-map-delay on-region-click!)]
          (view/set-properties! view-node nil
                                (cond-> (assoc props :document-width document-width)
                                        was-scrolled-to-bottom? (assoc :scroll-y (data/scroll-to-bottom prev-layout (count (:lines props))))
                                        clear (assoc :cursor-ranges [data/document-start-cursor-range])
                                        clear (assoc :invalidated-row 0)
                                        clear (data/frame-cursor prev-layout)))))))
  (view/repaint-view! view-node elapsed-time {:cursor-visible false :editable false}))

(def ^:private console-grammar
  {:name "Console"
   :scope-name "source.console"
   :patterns [{:match #"^INFO:RESOURCE: (.+?) was successfully reloaded\."
               :name "console.reload.successful"}
              {:match #"^ERROR:.+?:"
               :name "console.error"}
              {:match #"^FATAL:.+?:"
               :name "console.fatal"}
              {:match #"^WARNING:.+?:"
               :name "console.warning"}
              {:match #"^INFO:.+?:"
               :name "console.info"}
              {:match #"^DEBUG:.+?:"
               :name "console.debug"}]})

(def ^:private console-color-scheme
  (let [^Color background-color (Color/valueOf "#27292D")
        ^Color selection-background-color (Color/valueOf "#264A8B")]
    (view/make-color-scheme
      [["console.reload.successful" (Color/valueOf "#33CC33")]
       ["console.error" (Color/valueOf "#FF6161")]
       ["console.fatal" (Color/valueOf "#FF6161")]
       ["console.warning" (Color/valueOf "#FF9A34")]
       ["console.info" (Color/valueOf "#CCCFD3")]
       ["console.debug" (Color/valueOf "#3B8CF8")]
       ["editor.error" (Color/valueOf "#FF6161")]
       ["editor.warning" (Color/valueOf "#FF9A34")]
       ["editor.info" (Color/valueOf "#CCCFD3")]
       ["editor.debug" (Color/valueOf "#3B8CF8")]
       ["editor.foreground" (Color/valueOf "#A2B0BE")]
       ["editor.background" background-color]
       ["editor.cursor" Color/TRANSPARENT]
       ["editor.gutter.eval.expression" (Color/valueOf "#DDDDDD")]
       ["editor.gutter.eval.error" (Color/valueOf "#FF6161")]
       ["editor.gutter.eval.result" (Color/valueOf "#52575C")]
       ["editor.selection.background" selection-background-color]
       ["editor.selection.background.inactive" (.interpolate selection-background-color background-color 0.25)]
       ["editor.selection.occurrence.outline" (Color/valueOf "#A2B0BE")]])))

(defn- openable-resource? [value]
  (and (resource/openable-resource? value)
       (resource/exists? value)))

(def ^:private resource->menu-item (comp ui/string->menu-item resource/proj-path))

(defn make-console! [graph workspace ^Tab console-tab ^GridPane console-grid-pane open-resource-fn prefs localization]
  (let [^Pane canvas-pane (.lookup console-grid-pane "#console-canvas-pane")
        canvas (Canvas. (.getWidth canvas-pane) (.getHeight canvas-pane))
        view-node (setup-view! (g/make-node! graph ConsoleNode)
                               (g/make-node! graph view/CodeEditorView
                                             :canvas canvas
                                             :canvas-width (.getWidth canvas)
                                             :canvas-height (.getHeight canvas)
                                             :color-scheme console-color-scheme
                                             :grammar console-grammar
                                             :gutter-view (ConsoleGutterView.)
                                             :highlighted-find-term (.getValue find-term-property)
                                             :line-height-factor 1.2
                                             :resize-reference :bottom))
        tool-bar (setup-tool-bar! (.lookup console-grid-pane "#console-tool-bar") view-node prefs localization)
        on-region-click! (fn on-region-click! [region ^MouseEvent event]
                           (when (= :resource-reference (:type region))
                             (let [open-resource! (fn open-resource! [resource]
                                                    (when (openable-resource? resource)
                                                      (let [opts (when-some [row (:row region)]
                                                                   {:cursor-range (data/Cursor->CursorRange (data/->Cursor row 0))})]
                                                        (open-resource-fn resource opts))))
                                   resource-candidates (into []
                                                             (comp (keep (partial workspace/find-resource workspace))
                                                                   (filter openable-resource?))
                                                             (:proj-path-candidates region))]
                               (case (count resource-candidates)
                                 0 nil
                                 1 (open-resource! (first resource-candidates))
                                 (ui/show-simple-context-menu-at-mouse! resource->menu-item open-resource! resource-candidates event)))))
        repainter (ui/->timer "repaint-console-view" (fn [_ elapsed-time _]
                                                       (when (and (.isSelected console-tab) (not (ui/ui-disabled?)))
                                                         (repaint-console-view! view-node workspace on-region-click! elapsed-time))))
        context-env {:clipboard (Clipboard/getSystemClipboard)
                     :term-field (.lookup tool-bar "#search-console")
                     :view-node view-node}]

    ;; Canvas stretches to fit view, and updates properties in view node.
    (ui/add-child! canvas-pane canvas)
    (.bind (.widthProperty canvas) (.widthProperty canvas-pane))
    (.bind (.heightProperty canvas) (.heightProperty canvas-pane))
    (ui/observe (.widthProperty canvas) (fn [_ _ width] (g/set-property! view-node :canvas-width width)))
    (ui/observe (.heightProperty canvas) (fn [_ _ height] (g/set-property! view-node :canvas-height height)))

    ;; Configure canvas.
    (doto canvas
      (.setFocusTraversable true)
      (.addEventFilter KeyEvent/KEY_PRESSED (ui/event-handler event (view/handle-key-pressed! view-node prefs event false)))
      (.addEventHandler MouseEvent/MOUSE_MOVED (ui/event-handler event (view/handle-mouse-moved! view-node prefs event)))
      (.addEventHandler MouseEvent/MOUSE_PRESSED (ui/event-handler event (view/handle-mouse-pressed! view-node event)))
      (.addEventHandler MouseEvent/MOUSE_DRAGGED (ui/event-handler event (view/handle-mouse-moved! view-node prefs event)))
      (.addEventHandler MouseEvent/MOUSE_RELEASED (ui/event-handler event (view/handle-mouse-released! view-node event)))
      (.addEventHandler MouseEvent/MOUSE_EXITED (ui/event-handler event (view/handle-mouse-exited! view-node event)))
      (.addEventHandler ScrollEvent/SCROLL (ui/event-handler event (view/handle-scroll! view-node false event))))

    ;; Configure contexts.
    (ui/context! console-grid-pane :console-grid-pane context-env nil)
    (ui/context! canvas :console-view context-env nil)

    ;; Highlight occurrences of search term.
    (let [find-term-setter (view/make-property-change-setter view-node :highlighted-find-term)]
      (.addListener find-term-property find-term-setter)

      ;; Ensure the focus-state property reflects the current input focus state.
      (let [^Scene scene (.getScene console-grid-pane)
            focus-owner-property (.focusOwnerProperty scene)
            focus-change-listener (view/make-focus-change-listener view-node console-grid-pane canvas)]
        (.addListener focus-owner-property focus-change-listener)

        ;; Remove callbacks if the console tab is closed.
        (ui/on-closed! console-tab (fn [_]
                                     (ui/timer-stop! repainter)
                                     (dispose-tool-bar! tool-bar)
                                     (.removeListener find-term-property find-term-setter)
                                     (.removeListener focus-owner-property focus-change-listener)))))

    ;; Start repaint timer.
    (ui/timer-start! repainter)
    view-node))

(defn routes [console-view]
  (let [console-node (g/node-value console-view :resource-node)]
    (assert (g/node-instance? ConsoleNode console-node))
    {"/console" {"GET" (bound-fn [_]
                         (g/node-value console-node :request-response))}}))
