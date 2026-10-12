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

(ns editor.properties-view-test
  (:require [cljfx.api :as fx]
            [cljfx.fx.scene :as fx.scene]
            [cljfx.fx.stage :as fx.stage]
            [cljfx.plorer :as plorer]
            [clojure.test :refer [deftest is]]
            [dynamo.graph :as g]
            [editor.fxui :as fxui]
            [editor.properties-view :as properties-view]
            [editor.types :as types]
            [editor.ui :as ui]
            [support.test-support :as test-support]
            [util.coll :as coll])
  (:import [javafx.beans.value ChangeListener]
           [javafx.scene Node Parent Scene]
           [javafx.scene.control Button Labeled ListCell ListView TextArea TextField]
           [javafx.scene.control.skin VirtualFlow]
           [javafx.scene.layout Priority VBox]
           [javafx.stage Stage]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(g/defnode PropertyValues
  (property values g/Any))

(defn- property-item [i]
  (let [key (keyword (str "property-" i))
        label (str "Property " i)]
    {:property-keyword key
     :label label
     :property {:key key
                :label label
                :edit-type {:type g/Str}
                :node-ids [1]
                :prop-kws [key]
                :values [(str "value-" i)]}}))

(defn- node-property-items [node-id]
  (into []
        (map-indexed
          (fn [i value]
            (-> (property-item i)
                (assoc-in [:property :key] [:values i])
                (assoc-in [:property :node-ids] [node-id])
                (assoc-in [:property :prop-kws] [:values])
                (assoc-in [:property :tooltip] "Property value")
                (assoc-in [:property :values] [value]))))
        (g/node-value node-id :values)))

(defn- list-description [items node-ids]
  {:fx/type properties-view/property-list-view
   :items items
   :context {}
   :localization-state (constantly "Help")
   :selection-provider (properties-view/->SelectionProvider node-ids)})

(defn- layout! [^ListView list]
  (.applyCss list)
  (.layout list))

(defn- populated-cells [^ListView list]
  (into []
        (filter #(and (instance? ListCell %)
                      (.getGraphic ^ListCell %)))
        (.lookupAll list ".list-cell")))

;; Verifies rendered category order, localization, independent label widths,
;; and omission of empty categories through the public properties pane.
(deftest property-items-test
  (ui/run-now
    (let [component (fx/create-component
                      {:fx/type properties-view/properties-pane-view
                       :context {}
                       :localization (atom (fn localize [message] (str "Localized " message)))
                       :displayed-node-properties
                       [{:node-id 1
                         :display-order [:first ["Group" :second] ["Empty"]]
                         :properties {:first {:node-id 1 :type g/Str :value "first" :label "WWWWWWWWWW"}
                                      :second {:node-id 1 :type g/Str :value "second" :label "I"}}}]})
          ^Parent root (fx/instance component)
          _scene (Scene. root 400.0 400.0)]
      (try
        (ui/apply-css! root)
        (.applyCss root)
        (.layout root)
        (let [^ListView list (.lookup root "#properties")
              _ (layout! list)
              ^VirtualFlow flow (.lookup list ".virtual-flow")
              labels (mapv (fn [index]
                             (let [graphic (.getGraphic ^ListCell (.getVisibleCell flow (int index)))]
                               (or (.lookup graphic ".property-label")
                                   (.lookup graphic ".property-category"))))
                           (range 3))]
          (is (= 3 (count (.getItems list))))
          (is (= ["Localized WWWWWWWWWW" "Localized Group" "Localized I"]
                 (mapv #(.getText ^Labeled %) labels)))
          (is (> (.getWidth ^Labeled (labels 0)) (.getWidth ^Labeled (labels 2)))))
        (finally
          (fx/delete-component component))))))

;; Guards against eagerly creating editors for thousands of offscreen properties
;; and verifies that recycled editors commit to their current property. Clearing
;; or shrinking the list also covers JavaFX's temporarily nil cell items.
(deftest virtual-property-cells-test
  (test-support/with-clean-system
    (let [initial-values (mapv #(str "value-" %) (range 10000))
          node-id (first (test-support/tx-nodes (g/make-node PropertyValues :values initial-values)))]
      (ui/run-now
        (let [items (node-property-items node-id)
              component (volatile! (fx/create-component
                                     (list-description items [node-id])))
              ^ListView list (fx/instance @component)
              scene (Scene. list 300.0 400.0)
              stage (doto (Stage.) (.setOpacity 0.0) (.setScene scene) (.show))]
          (try
            (ui/apply-css! list)
            (layout! list)
            (is (= 10000 (count (.getItems list))))
            (is (< 0 (count (.lookupAll list ".text-field")) 50))
            (is (< 0 (count (populated-cells list)) 50))

            (let [^VirtualFlow flow (.lookup list ".virtual-flow")
                  ^ListCell visible (.getFirstVisibleCell flow)
                  ^ListCell measured (.getCell flow 9999)]
              (is (= (.getHeight visible) (.getHeight measured)))
              (is (= (.getPadding visible) (.getPadding measured))))

            (.scrollTo list 9900)
            (layout! list)
            (let [^ListCell cell
                  (coll/first-where #(<= 9900 (.getIndex ^ListCell %))
                                    (populated-cells list))
                  ^TextField field (.lookup (.getGraphic cell) ".text-field")
                  index (.getIndex cell)]
              (is (= (str "value-" (.getIndex cell)) (.getText field)))
              (.requestFocus field)
              (.setText field "edited")
              (plorer/key-tap! scene :enter)
              (is (= (assoc initial-values index "edited") (g/node-value node-id :values))))
            (is (< 0 (count (.lookupAll list ".text-field")) 50))

            (vreset! component
                     (fx/advance-component @component (list-description [] [])))
            (layout! list)
            (is (coll/empty? (populated-cells list)))
            (vreset! component
                     (fx/advance-component @component
                                           (list-description [(items 0)] [node-id])))
            (layout! list)
            (is (= "value-0" (.getText ^TextField
                               (.lookup list ".text-field"))))
            (finally
              (.hide stage)
              (fx/delete-component @component))))))))

;; Verifies variable row heights and replacement of controls when the selected
;; nodes change, while preserving a control when its property value changes.
(deftest virtual-property-updates-test
  (ui/run-now
    (let [items [(property-item 0)
                 (assoc-in (property-item 1)
                           [:property :edit-type :type] :multi-line-text)]
          component (volatile! (fx/create-component
                                 (list-description items [1])))
          ^ListView list (fx/instance @component)
          _scene (Scene. list 300.0 400.0)]
      (try
        (ui/apply-css! list)
        (layout! list)
        (let [^TextField field (.lookup list ".text-field")
              ^TextArea area (.lookup list ".text-area")
              updated-items (assoc-in items [0 :property :values] ["updated"])]
          (is (< (.getHeight field) (.getHeight area)))
          (is (= -1.0 (.getFixedCellSize list)))
          (vreset! component
                   (fx/advance-component @component
                                         (list-description updated-items [1])))
          (layout! list)
          (is (identical? field (.lookup list ".text-field")))
          (is (= "updated" (.getText field)))

          (vreset! component
                   (fx/advance-component @component
                                         (list-description updated-items [2])))
          (layout! list)
          (is (not (identical? field (.lookup list ".text-field")))))
        (finally
          (fx/delete-component @component))))))

;; Selection follows the property when its value or position changes, and is
;; cleared when that property disappears or the selected scene nodes change.
(deftest virtual-property-selection-updates-test
  (let [items (mapv property-item (range 3))
        component (ui/run-now (volatile! (fx/create-component (list-description items [1]))))
        ^ListView list (fx/instance @component)
        scene (ui/run-now (Scene. list 300.0 400.0))
        ^Stage stage (ui/run-now
                       (let [stage (doto (Stage.)
                                     (.setOpacity 0.0)
                                     (.setScene scene)
                                     (.show))]
                         (ui/apply-css! list)
                         (layout! list)
                         stage))]
    (try
      (let [field (ui/run-now (.lookup (.getGraphic ^ListCell
                                         (.getVisibleCell ^VirtualFlow (.lookup list ".virtual-flow") 1))
                                       ".text-field"))]
        (plorer/mouse-click! scene (plorer/point field 0.5 0.5) :primary))
      (ui/run-now (is (= 1 (.getSelectedIndex (.getSelectionModel list)))))
      (let [updated-items (assoc-in items [1 :property :values] ["updated"])]
        (doseq [[next-items expected-index]
                [[updated-items 1]
                 [(assoc-in updated-items [0 :property :values] ["other update"]) 1]
                 [(into [(property-item 3)] updated-items) 2]
                 [[(items 2) (items 0) (updated-items 1)] 2]
                 [(subvec updated-items 1) 0]
                 [[(items 0) (items 2)] -1]]]
          (ui/run-now
            (vreset! component (fx/advance-component @component (list-description next-items [1])))
            (layout! list)
            (is (= expected-index (.getSelectedIndex (.getSelectionModel list))))
            (when-not (neg? (long expected-index))
              (is (= :property-1 (:property-keyword (.getSelectedItem (.getSelectionModel list)))))))))
      (ui/run-now
        (.select (.getSelectionModel list) 0))
      (ui/run-now
        (vreset! component (fx/advance-component @component (list-description items [2])))
        (is (= -1 (.getSelectedIndex (.getSelectionModel list)))))
      (finally
        (ui/run-now
          (.hide stage)
          (fx/delete-component @component))))))

;; A pending text edit must reach its original property when scrolling recycles
;; the focused cell, just as it does when Tab moves focus to another editor.
(deftest virtual-property-scroll-commit-test
  (ui/run-now
    ;; Keep the isolated graph bound during native JavaFX focus-cleanup pulses.
    (test-support/with-clean-system
      (let [focused (promise)
            committed (promise)
            initial-values (mapv #(str "value-" %) (range 100))
            node-id (first (test-support/tx-nodes (g/make-node PropertyValues :values initial-values)))]
        (add-watch g/*the-system* ::scroll-commit
                   (fn on-property-committed [_ _ _ _]
                     (let [values (g/node-value node-id :values (g/make-evaluation-context))]
                       (when (not= initial-values values)
                         (deliver committed values)))))
        (try
          (let [result
                (fxui/show-stateless-dialog-and-await-result!
                  (fn property-test-dialog [finish!]
                    (let [run-scenario
                          (fn run-scroll-scenario [list scene field]
                            (finish!
                              (try
                                (is (= true (deref focused 10000 :timeout)))
                                (ui/run-now (.setText ^TextField field "pending"))
                                (plorer/scroll! scene (plorer/point list 0.5 0.5) 0 -1000000)
                                (ui/run-now (layout! list))
                                (deref committed 10000 :timeout)
                                (catch Throwable exception exception))))]
                      {:fx/type fx/ext-on-instance-lifecycle
                       :on-created
                       (fn on-test-stage-created [^Stage stage]
                         (let [scene (.getScene stage)
                               ^ListView list (.getRoot scene)]
                           (ui/apply-css! list)
                           (layout! list)
                           (let [^TextField field (.lookup (.getGraphic ^ListCell
                                                             (.getVisibleCell ^VirtualFlow (.lookup list ".virtual-flow") 0))
                                                           ".text-field")]
                             (.addListener (.focusedProperty field)
                                           (reify ChangeListener
                                             (changed [_ _ _ value]
                                               (when value (deliver focused true)))))
                             (.requestFocus stage)
                             (.requestFocus field)
                             (when (.isFocused field) (deliver focused true))
                             ;; Start input after cljfx finishes rendering.
                             (ui/do-run-later
                               (fn start-scroll-scenario []
                                 (future (run-scenario list scene field)))))))
                       :desc {:fx/type fx.stage/lifecycle
                              :showing true
                              :opacity 0.0
                              :width 300.0
                              :height 400.0
                              :scene {:fx/type fx.scene/lifecycle
                                      :root (list-description (node-property-items node-id) [node-id])}}})))]
            (when (instance? Throwable result)
              (throw result))
            (is (= (assoc initial-values 0 "pending") result)))
          (finally
            (remove-watch g/*the-system* ::scroll-commit)))))))

;; Tab must continue from the logical field after its row is recycled, even
;; within a tuple, instead of starting at the newly visible viewport position.
;; Assert that scrolling moved the original row offscreen before pressing Tab.
(deftest virtual-property-navigation-after-scroll-test
  (doseq [[row control-index keys expected-row expected-text]
          [[4 1 [:tab] 4 "3.0"]
           [4 1 [:shift :tab] 4 "1.0"]
           [4 2 [:tab] 5 "1.0"]
           [4 0 [:shift :tab] 3 "3.0"]
           [0 0 [:shift :tab] nil :before]
           [99 2 [:tab] nil :after]]
          scroll-mode [:gradual :fling]]
    (let [{:keys [component list scene stage before after]}
          (ui/run-now
            (let [items (mapv (fn [i]
                                (-> (property-item i)
                                    (assoc-in [:property :edit-type :type] types/Vec3)
                                    (assoc-in [:property :values] [[1.0 2.0 3.0]])))
                              (range 100))
                  component (fx/create-component (list-description items [1]))
                  ^ListView list (fx/instance component)
                  before (Button. "Before")
                  after (Button. "After")
                  root (VBox. ^Node/1 (into-array Node [before list after]))
                  scene (Scene. root 400.0 400.0)
                  stage (doto (Stage.) (.setOpacity 0.0) (.setScene scene) (.show))]
              (VBox/setVgrow list Priority/ALWAYS)
              (ui/apply-css! root)
              (.applyCss root)
              (.layout root)
              (layout! list)
              {:component component
               :list list
               :scene scene
               :stage stage
               :before before
               :after after}))]
      (try
        (let [field (ui/run-now
                      (let [^VirtualFlow flow (.lookup ^ListView list ".virtual-flow")]
                        (.scrollTo flow (int row))
                        (layout! list)
                        (nth (sort-by #(.getMinX (.localToScene ^Node % (.getLayoutBounds ^Node %)))
                                      (.lookupAll (.getGraphic ^ListCell (.getVisibleCell flow (int row)))
                                                  ".text-field"))
                             control-index)))]
          (dotimes [_ 2]
            (plorer/mouse-click! scene (plorer/point field 0.5 0.5) :primary)))
        (let [initial-position (ui/run-now
                                 (.getPosition ^VirtualFlow (.lookup ^ListView list ".virtual-flow")))]
          (dotimes [_ (if (= :fling scroll-mode) 1 30)]
            (plorer/scroll! scene (plorer/point list 0.5 0.5) 0
                            (* (if (= row 99) 1 -1) (if (= :fling scroll-mode) 1000000 45)))
            (ui/run-now (layout! list)))
          (ui/run-now
            (let [^VirtualFlow flow (.lookup ^ListView list ".virtual-flow")]
              (is (not= initial-position (.getPosition flow)) "Scrolling must move the viewport")
              (is (nil? (.getVisibleCell flow (int row))) "The original row must be offscreen")
              (is (= row (.getSelectedIndex (.getSelectionModel ^ListView list)))))))
        (plorer/key-chord! scene keys)
        (ui/run-now
          (let [owner (.getFocusOwner ^Scene scene)]
            (if-not expected-row
              (is (identical? ({:before before :after after} expected-text) owner))
              (do
                (is (instance? TextField owner))
                (is (= expected-row (.getSelectedIndex (.getSelectionModel ^ListView list))))
                (when (instance? TextField owner)
                  (is (= expected-text (.getText ^TextField owner)))
                  (let [viewport (.lookup ^ListView list ".clipped-container")
                        viewport-bounds (.localToScene viewport (.getLayoutBounds viewport))
                        owner-bounds (.localToScene ^TextField owner (.getLayoutBounds ^TextField owner))]
                    (is (<= (.getMinY viewport-bounds) (.getMinY owner-bounds)))
                    (is (<= (.getMaxY owner-bounds) (.getMaxY viewport-bounds)))))))))
        (plorer/mouse-click! scene (plorer/point before 0.5 0.5) :primary)
        (ui/run-now (is (identical? before (.getFocusOwner ^Scene scene))))
        (finally
          (ui/run-now
            (.hide ^Stage stage)
            (fx/delete-component component)))))))

;; Verifies that Tab and Shift+Tab traverse editors beyond the visible cells
;; and keep the focused editor entirely inside the viewport without moving
;; rows that are already visible. Traversal can leave either end of the list.
(deftest virtual-property-keyboard-navigation-test
  (ui/run-now
    (let [component (fx/create-component
                      (list-description (mapv property-item (range 100)) [1]))
          ^ListView list (fx/instance component)
          before (Button. "Before")
          after (Button. "After")
          root (VBox. ^Node/1 (into-array Node [before list after]))
          scene (Scene. root 300.0 400.0)
          stage (doto (Stage.)
                  (.setOpacity 0.0)
                  (.setScene scene)
                  (.show))]
      (try
        (VBox/setVgrow list Priority/ALWAYS)
        (ui/apply-css! root)
        (.applyCss root)
        (.layout root)
        (layout! list)
        (let [^ListCell first-cell
              (coll/first-where #(zero? (.getIndex ^ListCell %))
                                (populated-cells list))]
          (.requestFocus (.lookup (.getGraphic first-cell) ".text-field")))
        (let [^VirtualFlow flow (.lookup list ".virtual-flow")
              position (.getPosition flow)]
          (plorer/key-tap! scene :tab)
          (is (= position (.getPosition flow)))
          (plorer/key-chord! scene [:shift :tab]))
        (dotimes [_ 20]
          (plorer/key-tap! scene :tab)
          (layout! list))
        (let [owner (.getFocusOwner scene)]
          (is (instance? TextField owner))
          (when (instance? TextField owner)
            (is (= "value-20" (.getText ^TextField owner)))
            (is (= 20 (.getSelectedIndex (.getSelectionModel list))))
            (let [viewport (.lookup list ".clipped-container")
                  viewport-bounds (.localToScene viewport (.getLayoutBounds viewport))
                  owner-bounds (.localToScene ^TextField owner (.getLayoutBounds ^TextField owner))]
              (is (<= (.getMinY viewport-bounds) (.getMinY owner-bounds)))
              (is (<= (.getMaxY owner-bounds) (.getMaxY viewport-bounds))))))

        (dotimes [_ 20]
          (plorer/key-chord! scene [:shift :tab])
          (layout! list))
        (let [owner (.getFocusOwner scene)]
          (is (instance? TextField owner))
          (when (instance? TextField owner)
            (is (= "value-0" (.getText ^TextField owner)))))

        (plorer/key-chord! scene [:shift :tab])
        (is (identical? before (.getFocusOwner scene)))
        (plorer/scroll! scene (plorer/point list 0.5 0.5) 0 -1000000)
        (layout! list)
        (.requestFocus (.lookup (.getGraphic ^ListCell
                                  (coll/first-where #(= 99 (.getIndex ^ListCell %))
                                                    (populated-cells list)))
                                ".text-field"))
        (plorer/key-tap! scene :tab)
        (is (identical? after (.getFocusOwner scene)))
        (finally
          (.hide stage)
          (fx/delete-component component))))))

;; Multiline editors retain plain Tab for indentation while Shift+Tab and
;; Ctrl+Tab traverse properties, including offscreen rows and category headers.
(deftest virtual-property-multiline-navigation-test
  (ui/run-now
    (let [items [(property-item 0)
                 (assoc-in (property-item 1) [:property :edit-type :type] :multi-line-text)
                 {:category-title "Category"}
                 (property-item 2)]
          component (fx/create-component (list-description items [1]))
          ^ListView list (fx/instance component)
          scene (Scene. list 300.0 160.0)
          stage (doto (Stage.) (.setOpacity 0.0) (.setScene scene) (.show))]
      (try
        (ui/apply-css! list)
        (layout! list)
        (let [^VirtualFlow flow (.lookup list ".virtual-flow")
              ^TextField first-field (.lookup (.getGraphic ^ListCell (.getVisibleCell flow 0)) ".text-field")]
          (.requestFocus first-field)
          (plorer/key-tap! scene :tab)
          (let [^TextArea area (.getFocusOwner scene)]
            (is (instance? TextArea area))
            (.positionCaret area (.getLength area))
            (plorer/key-tap! scene :tab)
            (is (identical? area (.getFocusOwner scene)))
            (is (= "value-1\t" (.getText area)))
            (plorer/key-chord! scene [:shift :tab])
            (is (= "value-0" (.getText ^TextField (.getFocusOwner scene))))
            (plorer/key-tap! scene :tab)
            (plorer/key-chord! scene [:control :tab])
            (is (= "value-2" (.getText ^TextField (.getFocusOwner scene))))
            (is (= 3 (.getSelectedIndex (.getSelectionModel list))))
            (plorer/key-chord! scene [:shift :tab])
            (is (instance? TextArea (.getFocusOwner scene)))
            (plorer/key-chord! scene [:control :shift :tab])
            (is (= "value-0" (.getText ^TextField (.getFocusOwner scene))))))
        (finally
          (.hide stage)
          (fx/delete-component component))))))

;; Guards against Tab jumping back and forth between GUI tuple fields. Inputs
;; run between FX tasks so queued selection renders execute during traversal.
(deftest virtual-property-tuple-navigation-test
  (let [fixture (ui/run-now
                  (let [items (mapv (fn [i]
                                      (-> (property-item i)
                                          (assoc-in [:property :edit-type :type] types/Vec3)
                                          (assoc-in [:property :values] [[1.0 2.0 3.0]])))
                                    (range 30))
                        component (fx/create-component (list-description items [1]))
                        ^ListView list (fx/instance component)
                        scene (Scene. list 400.0 400.0)
                        stage (doto (Stage.)
                                (.setOpacity 0.0)
                                (.setScene scene)
                                (.show))]
                    (ui/apply-css! list)
                    (layout! list)
                    {:component component :list list :scene scene :stage stage}))
        ^ListView list (:list fixture)
        ^Scene scene (:scene fixture)]
    (try
      (let [field (ui/run-now
                    (let [^VirtualFlow flow (.lookup list ".virtual-flow")]
                      (.lookup (.getGraphic ^ListCell (.getVisibleCell flow 0))
                               ".text-field")))]
        (plorer/mouse-click! scene (plorer/point field 0.5 0.5) :primary))
      (doseq [[forward steps] [[true (range 1 61)] [false (range 59 -1 -1)]]]
        (reduce
          (fn [previous-position ^long step]
            (if forward
              (plorer/key-tap! scene :tab)
              (plorer/key-chord! scene [:shift :tab]))
            (let [[position selected-index text]
                  (ui/run-now
                    (let [^VirtualFlow flow (.lookup list ".virtual-flow")
                          owner (.getFocusOwner scene)]
                      [(.getPosition flow)
                       (.getSelectedIndex (.getSelectionModel list))
                       (when (instance? TextField owner)
                         (.getText ^TextField owner))]))]
              (is (= (quot step 3) selected-index))
              (is (= (["1.0" "2.0" "3.0"] (rem step 3)) text))
              (is ((if forward <= >=) previous-position position))
              position))
          (ui/run-now (.getPosition ^VirtualFlow (.lookup list ".virtual-flow")))
          steps))
      (finally
        (ui/run-now
          (.hide ^Stage (:stage fixture))
          (fx/delete-component (:component fixture)))))))

;; Guards against blank rows at the end of a rapid scroll and focus remaining
;; on an editor whose cell has been recycled for a different property. Small
;; scrolls retain the focused text selection; offscreen rows retain selection.
(deftest virtual-property-fast-scroll-test
  (ui/run-now
    (let [component (fx/create-component
                      (list-description (mapv property-item (range 10000)) [1]))
          ^ListView list (fx/instance component)
          scene (Scene. list 300.0 400.0)
          stage (doto (Stage.)
                  (.setOpacity 0.0)
                  (.setScene scene)
                  (.show))]
      (try
        (ui/apply-css! list)
        (layout! list)
        (dotimes [_ 20]
          (plorer/scroll! scene (plorer/point list 0.5 0.5) 0 1000000)
          (layout! list)
          (let [^ListCell first-cell
                (coll/first-where #(zero? (.getIndex ^ListCell %))
                                  (populated-cells list))
                ^TextField field (.lookup (.getGraphic first-cell) ".text-field")]
            (plorer/mouse-click! scene (plorer/point field 0.5 0.5) :primary)
            (.selectAll field)
            (plorer/scroll! scene (plorer/point list 0.5 0.5) 0 -10)
            (is (identical? field (.getFocusOwner scene)))
            (is (= "value-0" (.getSelectedText field))))
          (plorer/scroll! scene (plorer/point list 0.5 0.5) 0 -1000000)
          (layout! list)
          (let [cells (into []
                            (filter #(and (.isVisible ^ListCell %)
                                          (not (.isEmpty ^ListCell %))))
                            (.lookupAll list ".list-cell"))]
            (when-let [^ListCell focused-cell
                       (ui/closest-node-of-type ListCell (.getFocusOwner scene))]
              (is (= :property-0 (:property-keyword (.getItem focused-cell)))))
            (is (= 0 (.getSelectedIndex (.getSelectionModel list))))
            (is (coll/every? #(.getGraphic ^ListCell %) cells))
            (is (<= (.getLayoutY ^ListCell (apply min-key #(.getIndex ^ListCell %) cells)) 0.0))
            (is (= 9999 (transduce (map #(.getIndex ^ListCell %)) max -1 cells)))))
        (finally
          (.hide stage)
          (fx/delete-component component))))))
