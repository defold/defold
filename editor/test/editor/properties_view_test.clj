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
            [cljfx.fx.text-field :as fx.text-field]
            [cljfx.lifecycle :as fx.lifecycle]
            [cljfx.plorer :as plorer]
            [clojure.test :refer [deftest is testing]]
            [dynamo.graph :as g]
            [editor.fxui :as fxui]
            [editor.properties-view :as properties-view]
            [editor.types :as types]
            [editor.ui :as ui]
            [support.test-support :as test-support]
            [util.coll :as coll])
  (:import [javafx.animation PauseTransition]
           [javafx.application Platform]
           [javafx.beans.value ChangeListener]
           [javafx.scene Node Parent Scene]
           [javafx.scene.control Button Labeled TextArea TextField]
           [javafx.scene.layout Priority Region VBox]
           [javafx.stage Stage]
           [javafx.util Duration]
           [org.fxmisc.flowless Cell VirtualFlow]))

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

(defn- layout! [^Parent list]
  (.applyCss list)
  (.layout list))

(defn- property-flow
  ^VirtualFlow [^Parent root]
  (.lookup root ".property-flow"))

(defn- visible-row
  ^Node [^VirtualFlow flow index]
  (some-> (.orElse (.getCellIfVisible flow (int index)) nil) Cell/.getNode))

(defn- viewport-position
  ^double [^VirtualFlow flow]
  (let [index (.getFirstVisibleIndex flow)
        row (visible-row flow index)]
    (- (* index (.getHeight (.getLayoutBounds row))) (.getLayoutY row))))

(defn- await-uniform-flow-layout! [root]
  ;; Flowless debounces its size/scroll estimates. Wait for those observables
  ;; to agree with the actual uniform-height fixture, allowing JavaFX pulses.
  (ui/run-now
    (layout! root)
    (let [flow (property-flow root)
          length (.totalHeightEstimateProperty flow)
          offset (.estimatedScrollYProperty flow)
          ready? (fn flow-ready? []
                   (and (pos? (double (.getValue length)))
                        (<= (Math/abs (- (double (.getValue offset)) (viewport-position flow))) 1.0)))]
      (when-not (ready?)
        (let [loop-key (Object.)
              finished (volatile! false)
              finish! (fn finish-layout-wait! [result]
                        (when-not @finished
                          (vreset! finished true)
                          (ui/do-run-later
                            (fn exit-layout-wait! []
                              (Platform/exitNestedEventLoop loop-key result)))))
              listener (reify ChangeListener
                         (changed [_ _ _ _]
                           (when (ready?) (finish! true))))
              timeout (doto (PauseTransition. (Duration/seconds 10.0))
                        (.setOnFinished (fn on-layout-timeout [_] (finish! false))))]
          (.addListener length listener)
          (.addListener offset listener)
          (.play timeout)
          (try
            (when-not (Platform/enterNestedEventLoop loop-key)
              (throw (ex-info "Flowless layout did not settle" {})))
            (finally
              (.stop timeout)
              (.removeListener length listener)
              (.removeListener offset listener))))))))

(defn- scroll! [scene list delta]
  (await-uniform-flow-layout! list)
  (plorer/scroll! scene (plorer/point list 0.5 0.5) 0 delta)
  (await-uniform-flow-layout! list))

(defn- populated-cells [root]
  (mapv Cell/.getNode (.visibleCells (property-flow root))))

(defn- row-index [^Node row]
  (some-> (.getUserData row) name (subs 9) parse-long))

(defn- selected-index [^Parent root]
  (when-let [selected-row (.lookup root ".property-cell:selected")]
    (let [flow (property-flow root)]
      (+ (.getFirstVisibleIndex flow)
         (long (coll/first-index-where #(identical? selected-row %) (populated-cells root)))))))

(defn- focused-index [^Scene scene]
  (some-> (ui/closest-node-with-style "property-cell" (.getFocusOwner scene)) row-index))

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
        (let [^Parent list (.lookup root "#properties")
              _ (layout! list)
              ^VirtualFlow flow (property-flow list)
              labels (mapv (fn [index]
                             (let [graphic (visible-row flow index)]
                               (or (.lookup graphic ".property-label")
                                   (.lookup graphic ".property-category"))))
                           (range 3))]
          (is (= 2 (.getLastVisibleIndex flow)))
          (is (= ["Localized WWWWWWWWWW" "Localized Group" "Localized I"]
                 (mapv #(.getText ^Labeled %) labels)))
          (is (> (.getWidth ^Labeled (labels 0)) (.getWidth ^Labeled (labels 2)))))
        (finally
          (fx/delete-component component))))))

;; Verifies that a panel created with no selection can later display properties.
;; Guards against the Flowless wrapper treating its initial items as a callback.
(deftest virtual-property-empty-initial-state-test
  (ui/run-now
    (let [component (volatile! (fx/create-component (list-description [] [])))
          ^Parent root (fx/instance @component)
          _scene (Scene. root 400.0 400.0)]
      (try
        (ui/apply-css! root)
        (layout! root)
        (is (coll/empty? (populated-cells root)))
        (vreset! component
                 (fx/advance-component @component
                                       (list-description [(property-item 0)] [1])))
        (layout! root)
        (is (= 1 (count (populated-cells root))))
        (is (= "value-0" (.getText ^TextField (.lookup root ".text-field"))))
        (finally
          (fx/delete-component @component))))))

;; Guards against eagerly creating editors for thousands of offscreen properties
;; and verifies that newly visible editors commit to their own property.
;; Clearing and shrinking the list must also dispose offscreen controls.
(deftest virtual-property-cells-test
  (test-support/with-clean-system
    (let [initial-values (mapv #(str "value-" %) (range 10000))
          node-id (first (test-support/tx-nodes (g/make-node PropertyValues :values initial-values)))]
      (ui/run-now
        (let [items (node-property-items node-id)
              component (volatile! (fx/create-component
                                     (list-description items [node-id])))
              ^Parent list (fx/instance @component)
              scene (Scene. list 300.0 400.0)
              stage (doto (Stage.) (.setOpacity 0.0) (.setScene scene) (.show))]
          (try
            (ui/apply-css! list)
            (layout! list)
            (is (< 0 (count (.lookupAll list ".text-field")) 50))
            (is (< 0 (count (populated-cells list)) 50))

            (let [^VirtualFlow flow (property-flow list)
                  ^Region visible (visible-row flow 0)
                  ^Region measured (.getNode ^Cell (.getCell flow 9999))]
              (is (= (.getHeight visible) (.getHeight measured)))
              (is (= (.getPadding visible) (.getPadding measured))))

            (.show (property-flow list) (int 9900))
            (layout! list)
            (let [^Region cell
                  (coll/first-where #(<= 9900 (long (row-index %)))
                                    (populated-cells list))
                  ^TextField field (.lookup cell ".text-field")
                  index (row-index cell)]
              (is (= (str "value-" (row-index cell)) (.getText field)))
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
          ^Parent list (fx/instance @component)
          _scene (Scene. list 300.0 400.0)]
      (try
        (ui/apply-css! list)
        (layout! list)
        (let [^TextField field (.lookup list ".text-field")
              ^TextArea area (.lookup list ".text-area")
              updated-items (assoc-in items [0 :property :values] ["updated"])]
          (is (< (.getHeight field) (.getHeight area)))
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
        ^Parent list (fx/instance @component)
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
      (let [field (ui/run-now (.lookup (visible-row (property-flow list) 1)
                                       ".text-field"))]
        (plorer/mouse-click! scene (plorer/point field 0.5 0.5) :primary))
      (ui/run-now (is (= 1 (selected-index list))))
      (let [updated-items (assoc-in items [1 :property :values] ["updated"])]
        (doseq [[next-items expected-index]
                [[updated-items 1]
                 [(assoc-in updated-items [0 :property :values] ["other update"]) 1]
                 [(into [(property-item 3)] updated-items) 2]
                 [[(items 2) (items 0) (updated-items 1)] 2]
                 [(subvec updated-items 1) 0]
                 [[(items 0) (items 2)] nil]
                 [updated-items nil]]]
          (ui/run-now
            (vreset! component (fx/advance-component @component (list-description next-items [1])))
            (layout! list)
            (is (= expected-index (selected-index list)))
            (when expected-index
              (is (= :property-1 (.getUserData (.lookup list ".property-cell:selected"))))))))
      (let [field (ui/run-now (.lookup (visible-row (property-flow list) 0) ".text-field"))]
        (plorer/mouse-click! scene (plorer/point field 0.5 0.5) :primary))
      (ui/run-now
        (vreset! component (fx/advance-component @component (list-description items [2])))
        (is (nil? (selected-index list))))
      (finally
        (ui/run-now
          (.hide stage)
          (fx/delete-component @component))))))

;; A pending text edit must reach its original property when scrolling disposes
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
                                (scroll! scene list -1000000)
                                (ui/run-now (layout! list))
                                (deref committed 10000 :timeout)
                                (catch Throwable exception exception))))]
                      {:fx/type fx/ext-on-instance-lifecycle
                       :on-created
                       (fn on-test-stage-created [^Stage stage]
                         (let [scene (.getScene stage)
                               ^Parent list (.getRoot scene)]
                           (ui/apply-css! list)
                           (layout! list)
                           (let [^TextField field (.lookup (visible-row (property-flow list) 0)
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

;; Tab must continue from the logical field after its row is disposed, even
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
    (testing (str "row " row ", field " control-index ", keys " keys ", " scroll-mode)
      (let [{:keys [component list scene stage before after]}
            (ui/run-now
              (let [items (mapv (fn [i]
                                  (-> (property-item i)
                                      (assoc-in [:property :edit-type :type] types/Vec3)
                                      (assoc-in [:property :values] [[1.0 2.0 3.0]])))
                                (range 100))
                    component (fx/create-component (list-description items [1]))
                    ^Parent list (fx/instance component)
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
                        (let [^VirtualFlow flow (property-flow list)]
                          (.show flow (int row))
                          (layout! list)
                          (nth (sort-by #(.getMinX (.localToScene ^Node % (.getLayoutBounds ^Node %)))
                                        (.lookupAll (visible-row flow row)
                                                    ".text-field"))
                               control-index)))]
            (dotimes [_ 2]
              (plorer/mouse-click! scene (plorer/point field 0.5 0.5) :primary))
            (ui/run-now (is (identical? field (.getFocusOwner ^Scene scene)))))
          (let [initial-position (ui/run-now
                                   (viewport-position (property-flow list)))]
            (dotimes [_ (if (= :fling scroll-mode) 1 30)]
              (await-uniform-flow-layout! list)
              (plorer/scroll! scene (plorer/point list 0.5 0.5) 0
                              (* (if (= row 99) 1 -1) (if (= :fling scroll-mode) 1000000 45)))
              (ui/run-now (layout! list)))
            (ui/run-now
              (let [^VirtualFlow flow (property-flow list)]
                (is (not= initial-position (viewport-position flow)) "Scrolling must move the viewport")
                (is (nil? (visible-row flow row)) "The original row must be offscreen"))))
          (plorer/key-chord! scene keys)
          (ui/run-now
            (let [owner (.getFocusOwner ^Scene scene)]
              (if-not expected-row
                (is (identical? ({:before before :after after} expected-text) owner))
                (do
                  (is (instance? TextField owner))
                  (is (= expected-row (focused-index scene)))
                  (when (instance? TextField owner)
                    (is (= expected-text (.getText ^TextField owner)))
                    (let [viewport (property-flow list)
                          viewport-bounds (.localToScene viewport (.getLayoutBounds viewport))
                          owner-bounds (.localToScene ^TextField owner (.getLayoutBounds ^TextField owner))]
                      (is (<= (.getMinY viewport-bounds) (.getMinY owner-bounds)))
                      (is (<= (.getMaxY owner-bounds) (.getMaxY viewport-bounds)))))))))
          (plorer/mouse-click! scene (plorer/point before 0.5 0.5) :primary)
          (ui/run-now (is (identical? before (.getFocusOwner ^Scene scene))))
          (finally
            (ui/run-now
              (.hide ^Stage stage)
              (fx/delete-component component))))))))

;; Verifies that Tab and Shift+Tab traverse editors beyond the visible cells
;; and keep the focused editor entirely inside the viewport without moving
;; rows that are already visible. Traversal can leave either end of the list.
(deftest virtual-property-keyboard-navigation-test
  (ui/run-now
    (let [component (fx/create-component
                      (list-description (mapv property-item (range 100)) [1]))
          ^Parent list (fx/instance component)
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
        (let [^Region first-cell
              (coll/first-where #(zero? (long (row-index %)))
                                (populated-cells list))]
          (.requestFocus (.lookup first-cell ".text-field")))
        (let [^VirtualFlow flow (property-flow list)
              position (viewport-position flow)]
          (plorer/key-tap! scene :tab)
          (is (= position (viewport-position flow)))
          (plorer/key-chord! scene [:shift :tab]))
        (dotimes [_ 20]
          (plorer/key-tap! scene :tab)
          (layout! list))
        (let [owner (.getFocusOwner scene)]
          (is (instance? TextField owner))
          (when (instance? TextField owner)
            (is (= "value-20" (.getText ^TextField owner)))
            (is (= 20 (focused-index scene)))
            (let [viewport (property-flow list)
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
        (scroll! scene list -1000000)
        (layout! list)
        (.requestFocus (.lookup (visible-row (property-flow list) 99) ".text-field"))
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
          ^Parent list (fx/instance component)
          scene (Scene. list 300.0 160.0)
          stage (doto (Stage.) (.setOpacity 0.0) (.setScene scene) (.show))]
      (try
        (ui/apply-css! list)
        (layout! list)
        (let [^VirtualFlow flow (property-flow list)
              ^TextField first-field (.lookup (visible-row flow 0) ".text-field")]
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
            (is (= 2 (focused-index scene)))
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
                        ^Parent list (fx/instance component)
                        scene (Scene. list 400.0 400.0)
                        stage (doto (Stage.)
                                (.setOpacity 0.0)
                                (.setScene scene)
                                (.show))]
                    (ui/apply-css! list)
                    (layout! list)
                    {:component component :list list :scene scene :stage stage}))
        ^Parent list (:list fixture)
        ^Scene scene (:scene fixture)]
    (try
      (let [field (ui/run-now
                    (let [^VirtualFlow flow (property-flow list)]
                      (.lookup (visible-row flow 0)
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
                    (let [^VirtualFlow flow (property-flow list)
                          owner (.getFocusOwner scene)]
                      [(viewport-position flow)
                       (focused-index scene)
                       (when (instance? TextField owner)
                         (.getText ^TextField owner))]))]
              (is (= (quot step 3) selected-index))
              (is (= (["1.0" "2.0" "3.0"] (rem step 3)) text))
              (is ((if forward <= >=) previous-position position))
              position))
          (ui/run-now (viewport-position (property-flow list)))
          steps))
      (finally
        (ui/run-now
          (.hide ^Stage (:stage fixture))
          (fx/delete-component (:component fixture)))))))

;; Guards against blank rows at the end of a rapid scroll and focus remaining
;; on an editor whose cell has left the viewport. Small scrolls retain text
;; selection; offscreen rows retain property selection.
(deftest virtual-property-fast-scroll-test
  (ui/run-now
    (let [component (fx/create-component
                      (list-description (mapv property-item (range 10000)) [1]))
          ^Parent list (fx/instance component)
          scene (Scene. list 300.0 400.0)
          stage (doto (Stage.)
                  (.setOpacity 0.0)
                  (.setScene scene)
                  (.show))]
      (try
        (ui/apply-css! list)
        (layout! list)
        (dotimes [_ 20]
          (scroll! scene list 1000000)
          (layout! list)
          (let [^Region first-cell
                (coll/first-where #(zero? (long (row-index %)))
                                  (populated-cells list))
                ^TextField field (.lookup first-cell ".text-field")]
            (plorer/mouse-click! scene (plorer/point field 0.5 0.5) :primary)
            (.selectAll field)
            (scroll! scene list -10)
            (is (identical? field (.getFocusOwner scene)))
            (is (= "value-0" (.getSelectedText field))))
          (scroll! scene list -1000000)
          (layout! list)
          (let [flow (property-flow list)
                cells (populated-cells list)]
            (when-let [focused-row (ui/closest-node-with-style "property-cell" (.getFocusOwner scene))]
              (is (= :property-0 (.getUserData focused-row))))
            (is (coll/every? #(.lookup ^Node % ".text-field") cells))
            (is (<= (.getMinY (.localToScene ^Node (first cells) (.getLayoutBounds ^Node (first cells))))
                    (.getMinY (.localToScene flow (.getLayoutBounds flow)))))
            (is (= 9999 (.getLastVisibleIndex flow)))))
        (finally
          (.hide stage)
          (fx/delete-component component))))))

;; Reopening the panel must release each old editor exactly once, retain only
;; visible controls, and keep selection visible after scrolling away and back.
(deftest virtual-property-reopen-test
  (let [events (atom {})
        text-field-lifecycle
        (fx.lifecycle/wrap-log
          fx.text-field/lifecycle
          (fn on-text-field-lifecycle [event & _]
            (swap! events update event (fnil inc 0))))
        opts {:fx.opt/type->lifecycle
              (fn resolve-test-type [type]
                (if (identical? fx.text-field/lifecycle type)
                  text-field-lifecycle
                  (or (fx/keyword->lifecycle type) (fx/fn->lifecycle type))))}]
    (dotimes [_ 3]
      (ui/run-now
        (let [component (fx/create-component
                          (list-description (mapv property-item (range 10000)) [1]) opts)
              ^Parent root (fx/instance component)
              scene (Scene. root 300.0 400.0)
              stage (doto (Stage.) (.setOpacity 0.0) (.setScene scene) (.show))]
          (try
            (ui/apply-css! root)
            (await-uniform-flow-layout! root)
            (let [flow (property-flow root)
                  field (.lookup (visible-row flow 0) ".text-field")]
              (plorer/mouse-click! scene (plorer/point field 0.5 0.5) :primary)
              (scroll! scene root -1000000)
              (is (= 9999 (.getLastVisibleIndex flow)))
              (is (nil? (visible-row flow 0)))
              (is (< 0 (count (populated-cells root)) 50))
              (is (< 0 (- (long (get @events `fx.lifecycle/create 0))
                          (long (get @events `fx.lifecycle/delete 0))) 50))
              (scroll! scene root 1000000)
              (is (= 0 (selected-index root)))
              ;; Clicking a different visible field supersedes remembered focus.
              (let [field (.lookup (visible-row flow 3) ".text-field")]
                (plorer/mouse-click! scene (plorer/point field 0.5 0.5) :primary)
                (plorer/key-tap! scene :tab)
                (is (= 4 (focused-index scene))))
              (is (= (.getBackground ^Region root)
                     (.getBackground ^Region (visible-row flow 0)))))
            (finally
              (.hide stage)
              (fx/delete-component component opts)))))
      (is (= (get @events `fx.lifecycle/create) (get @events `fx.lifecycle/delete))))))

;; A key arriving immediately after a fling must reach the logical next field,
;; even before JavaFX's normal focus-cleanup pulse has had a chance to run.
(deftest virtual-property-tab-before-pulse-test
  (doseq [[keys expected-row] [[[:tab] 5] [[:shift :tab] 3]]]
    (ui/run-now
      (let [component (fx/create-component (list-description (mapv property-item (range 100)) [1]))
            ^Parent root (fx/instance component)
            scene (Scene. root 300.0 400.0)
            stage (doto (Stage.) (.setOpacity 0.0) (.setScene scene) (.show))]
        (try
          (ui/apply-css! root)
          (await-uniform-flow-layout! root)
          (let [flow (property-flow root)
                field (.lookup (visible-row flow 4) ".text-field")]
            (plorer/mouse-click! scene (plorer/point field 0.5 0.5) :primary)
            (plorer/scroll! scene (plorer/point root 0.5 0.5) 0 -1000000)
            (layout! root)
            (is (nil? (visible-row flow 4)))
            (plorer/key-chord! scene keys)
            (is (= expected-row (focused-index scene))))
          (finally
            (.hide stage)
            (fx/delete-component component)))))))
