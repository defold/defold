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

(ns editor.panes-test
  (:require [cljfx.api :as fx]
            [cljfx.fx.button :as fx.button]
            [cljfx.fx.label :as fx.label]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.handler :as handler]
            [editor.outline-view :as outline-view]
            [editor.panes :as panes]
            [editor.prefs :as prefs]
            [editor.ui :as ui]
            [integration.test-util :as test-util]
            [util.coll :as coll])
  (:import [clojure.lang IRef]
           [javafx.event Event]
           [javafx.scene Scene]
           [javafx.scene.control Button SplitPane Tab TabPane TextField TreeItem TreeView]
           [javafx.scene.input MouseButton MouseEvent]
           [javafx.scene.layout AnchorPane]
           [javafx.stage Stage]))

(set! *warn-on-reflection* true)

;; Extension replacements apply valid definitions atomically and report rejected
;; IDs afterward, preserving the initialized built-in definitions.
(deftest pane-registration-test
  (test-util/with-loaded-project
    (let [built-in {:id :test.built-in}

          extension
          ^{:proj-path "/first.editor_script"} {:id :test.extension}

          valid {:id :test.valid}
          conflicting (assoc extension :id (:id built-in))]
      (app-view/set-built-in-panes! app-view [built-in])
      (testing "A valid replacement updates only the extension vector"
        (is (= {} (app-view/set-extension-panes! app-view [extension])))
        (is (= {:built-in [built-in] :extension [extension]}
               (g/node-value app-view :pane-definitions))))
      (testing "Conflicts are reported after valid definitions are applied"
        (is (= {(:id built-in) [built-in conflicting]}
               (app-view/set-extension-panes! app-view [valid conflicting])))
        (is (= {:built-in [built-in] :extension [valid]}
               (g/node-value app-view :pane-definitions))))
      (testing "Duplicate extension IDs are all omitted"
        (is (= {(:id extension) [extension extension]}
               (app-view/set-extension-panes! app-view [extension valid extension])))
        (is (= {:built-in [built-in] :extension [valid]}
               (g/node-value app-view :pane-definitions))))
      (testing "An empty replacement removes all extension panes"
        (is (= {} (app-view/set-extension-panes! app-view [])))
        (is (= {:built-in [built-in] :extension []}
               (g/node-value app-view :pane-definitions)))))))

;; All contributions share five hosts, and hiding or moving tabs preserves each
;; full pane instance and its native input state instead of adding more splits.
(deftest grouped-pane-lifetime-test
  (ui/run-now
    (let [prefs (test-util/make-test-prefs)
          left (SplitPane.)
          right (SplitPane.)
          bottom (AnchorPane.)
          input (TextField. "Unsaved input")
          disposed-count (atom 0)
          label {:fx/type fx.label/lifecycle :text "Content"}

          descriptions
          [{:id :assets
            :title "Assets"
            :location :left-top
            :content-desc {:fx/type fx/ext-effect
                           :fn (fn assets-mounted []
                                 (fn dispose-assets [] (swap! disposed-count inc)))
                           :desc {:fx/type ui/ext-value :value input}}}
           {:id :pinned :title "Pinned" :location :left-top :content-desc label}
           {:id :changes :title "Changes" :location :left-bottom :content-desc label}
           {:id :outline :title "Outline" :location :right-top :content-desc label}
           {:id :properties :title "Properties" :location :right-bottom :content-desc label}
           {:id :variables :title "Variables" :location :right-bottom :content-desc label}
           {:id :console :title "Console" :location :bottom :content-desc label}]

          describe
          (fn describe [definitions hidden]
            {:fx/type panes/layout
             :panes definitions
             :hidden hidden
             :localization test-util/localization
             :prefs prefs
             :assets-split left
             :right-split right
             :bottom-container bottom})

          watches-before
          (count (.getWatches ^IRef test-util/localization))

          component
          (volatile! (fx/create-component (describe descriptions #{})))

          ^TabPane left-top (first (.getItems left))
          ^TabPane left-bottom (second (.getItems left))
          ^TabPane right-bottom (second (.getItems right))
          ^Tab first-tab (first (.getTabs left-top))
          ^Tab original-pinned-tab (second (.getTabs left-top))
          pinned-content (.getContent original-pinned-tab)
          ^Tab variable-tab (second (.getTabs right-bottom))]
      (try
        (is (= (inc watches-before) (count (.getWatches ^IRef test-util/localization))))
        (is (identical? input (.getContent first-tab)))
        (is (= ["tab" "assets-tab"] (vec (.getStyleClass first-tab))))
        (is (= [2 2] [(count (.getItems left)) (count (.getItems right))]))
        (.select (.getSelectionModel right-bottom) variable-tab)
        (is (= :variables (get (prefs/get prefs [:window :layout :selected-tabs]) :right-bottom)))
        (vswap! component fx/advance-component (describe descriptions #{:assets :pinned :properties}))
        (is (= 1 (count (.getItems left))))
        (is (identical? variable-tab (ui/selected-tab right-bottom)))
        (vswap! component fx/advance-component (describe descriptions #{:assets :pinned :changes :outline :properties :variables}))
        (is (= [0 0] [(count (.getItems left)) (count (.getItems right))]))
        (is (identical? input (.getContent first-tab)))
        (is (zero? @disposed-count))
        (vswap! component fx/advance-component
                (describe (mapv #(if (= :assets (:id %)) (assoc % :content-desc nil) %) descriptions) #{}))
        (is (nil? (.getTabPane first-tab)))
        (is (= 1 @disposed-count))
        (vswap! component fx/advance-component
                (describe (mapv #(if (= :pinned (:id %)) (assoc % :location :left-bottom) %) descriptions) #{}))
        (testing "Moving a pane changes its tab owner while preserving the content"
          (let [^Tab moved-tab
                (coll/first-where #(= "Pinned" (.getText ^Tab %)) (.getTabs left-bottom))]
            (is (identical? left-bottom (.getTabPane moved-tab)))
            (is (identical? pinned-content (.getContent moved-tab)))
            (is (nil? (.getTabPane original-pinned-tab)))
            (vswap! component fx/advance-component (describe descriptions #{}))
            (let [^Tab returned-tab
                  (coll/first-where #(= "Pinned" (.getText ^Tab %)) (.getTabs left-top))]
              (is (identical? left-top (.getTabPane returned-tab)))
              (is (identical? pinned-content (.getContent returned-tab)))
              (is (nil? (.getTabPane moved-tab))))))
        (is (not (identical? first-tab (first (.getTabs left-top)))))
        (is (identical? input (.getContent ^Tab (first (.getTabs left-top)))))
        (is (= "Unsaved input" (.getText input)))
        (is (= 1 @disposed-count))
        (is (= [2 2] [(count (.getItems left)) (count (.getItems right))]))
        (finally
          (fx/delete-component @component)))
      (is (= 2 @disposed-count))
      (is (= watches-before (count (.getWatches ^IRef test-util/localization)))))))

;; Prefs changes update tab selection and split positions without advancing the
;; layout from outside, and deleting the layout removes its prefs subscription.
(deftest pane-prefs-watcher-test
  (let [prefs (test-util/make-test-prefs)

        {:keys [component left host watches-before]}
        (ui/run-now
          (prefs/set! prefs [:window :layout :selected-tabs] {:left-top :assets :left-bottom :changes})
          (let [left (SplitPane.)
                watches-before (count (.getWatches ^IRef prefs/global-state))

                component
                (fx/create-component
                  {:fx/type panes/layout
                   :panes [{:id :assets
                            :title "Assets"
                            :location :left-top
                            :content-desc {:fx/type fx.label/lifecycle :text "Assets"}}
                           {:id :pinned
                            :title "Pinned"
                            :location :left-top
                            :content-desc {:fx/type fx.label/lifecycle :text "Pinned"}}
                           {:id :changes
                            :title "Changes"
                            :location :left-bottom
                            :content-desc {:fx/type fx.label/lifecycle :text "Changes"}}]
                   :hidden #{}
                   :localization test-util/localization
                   :prefs prefs
                   :assets-split left
                   :right-split (SplitPane.)
                   :bottom-container (AnchorPane.)})]
            {:component component
             :left left
             :host (first (.getItems left))
             :watches-before watches-before}))]
    (try
      (testing "Selection follows a prefs update without an external advance"
        (prefs/set! prefs [:window :layout :selected-tabs] {:left-top :pinned :left-bottom :changes})
        (ui/run-now
          (is (= "Pinned" (.getText ^Tab (ui/selected-tab host))))))
      (testing "Split positions follow a prefs update without an external advance"
        (prefs/set! prefs [:window :split-positions] {:assets-split [0.35]})
        (ui/run-now
          (is (= [0.35] (vec (.getDividerPositions ^SplitPane left))))))
      (testing "User selection still updates current prefs"
        (ui/run-now
          (is (= [false true] (mapv #(.isSelected ^Tab %) (.getTabs ^TabPane host))))
          (.select (.getSelectionModel ^TabPane host) (.get (.getTabs ^TabPane host) 0))
          (is (= :assets (get (prefs/get prefs [:window :layout :selected-tabs]) :left-top)))))
      (finally
        (ui/run-now (fx/delete-component component))))
    (is (= watches-before (count (.getWatches ^IRef prefs/global-state))))))

;; Automatic fallbacks preserve the preferred pane when its content disappears,
;; while explicit selection replaces that preference.
(deftest preferred-pane-selection-test
  (ui/run-now
    (let [prefs (test-util/make-test-prefs)
          left (SplitPane.)
          right (SplitPane.)
          bottom (AnchorPane.)
          label {:fx/type fx.label/lifecycle :text "Content"}

          describe
          (fn describe [properties-content]
            {:fx/type panes/layout
             :panes [{:id :properties :title "Properties" :location :right-bottom :content-desc properties-content}
                     {:id :notes :title "Notes" :location :right-bottom :content-desc label}]
             :hidden #{}
             :localization test-util/localization
             :prefs prefs
             :assets-split left
             :right-split right
             :bottom-container bottom})

          _ (prefs/set! prefs [:window :layout :selected-tabs] {:right-bottom :properties})
          component (volatile! (fx/create-component (describe label)))
          ^TabPane host (first (.getItems right))]
      (try
        (testing "An unavailable preferred pane is restored after a fallback"
          (is (= "Properties" (.getText ^Tab (ui/selected-tab host))))
          (vswap! component fx/advance-component (describe nil))
          (is (= "Notes" (.getText ^Tab (ui/selected-tab host))))
          (is (= :properties (get (prefs/get prefs [:window :layout :selected-tabs]) :right-bottom)))
          (vswap! component fx/advance-component (describe label))
          (is (= "Properties" (.getText ^Tab (ui/selected-tab host)))))
        (testing "Explicit selection replaces the preferred pane"
          (is (= [true false] (mapv #(.isSelected ^Tab %) (.getTabs host))))
          (.selectNext (.getSelectionModel host))
          (is (= "Notes" (.getText ^Tab (ui/selected-tab host))))
          (is (= :notes (get (prefs/get prefs [:window :layout :selected-tabs]) :right-bottom)))
          (vswap! component fx/advance-component (describe nil))
          (vswap! component fx/advance-component (describe label))
          (is (= "Notes" (.getText ^Tab (ui/selected-tab host))))
          (is (= :notes (get (prefs/get prefs [:window :layout :selected-tabs]) :right-bottom))))
        (finally
          (fx/delete-component @component))))))

;; ID-based activation reveals the pane and its region while individual hiding
;; persists independently of region toggles and automatically empty regions.
(deftest individual-pane-visibility-test
  (test-util/with-loaded-project
    (ui/run-now
      (let [prefs (test-util/make-test-prefs)
            root (ui/load-fxml "editor.fxml")

            scene
            (Scene. root)

            stage
            (doto (Stage.) (.setScene scene))

            input
            (TextField. "Retained")

            console-input (TextField.)
            curve-input (TextField.)
            properties-input (TextField.)
            console-descriptions (atom 0)
            console-disposals (atom 0)

            console-mounted
            (fn console-mounted []
              (fn dispose-console [] (swap! console-disposals inc)))

            assets
            {:id :assets
             :title "Assets"
             :location :left-top
             :content (constantly {:fx/type ui/ext-value :value input})}

            console
            {:id :console
             :title "Console"
             :location :bottom
             :content (fn describe-console [_]
                        (swap! console-descriptions inc)
                        {:fx/type fx/ext-effect
                         :fn console-mounted
                         :desc {:fx/type ui/ext-value :value console-input}})}

            curve
            {:id :curve-editor
             :title "Curve Editor"
             :location :bottom
             :content (constantly {:fx/type ui/ext-value :value curve-input})}

            properties
            {:id :properties
             :title "Properties"
             :location :right-bottom
             :content (constantly {:fx/type ui/ext-value :value properties-input})}

            contexts
            [(handler/->context :global {:main-stage stage :app-view app-view :prefs prefs})]]
        (.applyCss root)
        (.layout root)
        (prefs/set! prefs [:window :hidden-panes] #{:bottom})
        (prefs/set! prefs [:window :layout :hidden-panes] #{})
        (g/transact (g/set-property app-view :scene scene))
        (ui/context! root :global {:main-stage stage :app-view app-view :prefs prefs} nil)
        (app-view/set-built-in-panes! app-view [assets console curve properties])
        (app-view/initialize-panes! app-view scene prefs test-util/localization)
        (try
          (is (= 1 @console-descriptions))
          (is (zero? @console-disposals))
          (is (= [:assets :console :curve-editor :properties]
                 (mapv :user-data (test-util/handler-options :window.toggle-pane contexts nil))))
          (is (nil? (test-util/handler-options :window.toggle-pane contexts :properties)))
          (test-util/handler-run :window.toggle-pane contexts :properties)
          (is (nil? (.lookup scene "#right-pane")))
          (is (= #{:properties} (prefs/get prefs [:window :layout :hidden-panes])))
          (app-view/show-pane! scene :properties)
          (is (.lookup scene "#right-pane"))
          (is (identical? properties-input (.getContent ^Tab (ui/selected-tab (.lookup scene "#right-pane .tab-pane")))))
          (test-util/handler-run :window.toggle-right-pane contexts nil)
          (is (nil? (.lookup scene "#right-pane")))
          (is (= #{:bottom :right} (prefs/get prefs [:window :hidden-panes])))
          (is (= #{} (prefs/get prefs [:window :layout :hidden-panes])))
          (app-view/show-pane! scene :properties)
          (is (.lookup scene "#right-pane"))
          (is (= #{:bottom} (prefs/get prefs [:window :hidden-panes])))
          (test-util/handler-run :window.toggle-pane contexts :assets)
          (is (nil? (.lookup scene "#left-pane")))
          (is (= #{:bottom} (prefs/get prefs [:window :hidden-panes])))
          (is (pos? @console-descriptions))
          (app-view/show-pane! scene :console)
          (is (= #{} (prefs/get prefs [:window :hidden-panes])))
          (is (= #{:assets} (prefs/get prefs [:window :layout :hidden-panes])))
          (is (identical? console-input (.getContent ^Tab (ui/selected-tab (.lookup scene "#bottom-pane .tab-pane")))))
          (app-view/show-pane! scene :curve-editor)
          (let [descriptions-before @console-descriptions

                ^TabPane host
                (.lookup scene "#bottom-pane .tab-pane")

                ^Tab curve-tab
                (ui/selected-tab host)]
            (test-util/handler-run :window.toggle-bottom-pane contexts nil)
            (is (nil? (.lookup scene "#bottom-pane")))
            (is (identical? host (.getTabPane curve-tab)))
            (is (nil? (.getScene host)))
            (is (= (inc descriptions-before) @console-descriptions))
            (is (zero? @console-disposals)))
          (test-util/handler-run :window.toggle-bottom-pane contexts nil)
          (is (= #{} (prefs/get prefs [:window :hidden-panes])))
          (is (identical? curve-input (.getContent ^Tab (ui/selected-tab (.lookup scene "#bottom-pane .tab-pane")))))
          (app-view/show-pane! scene :assets)
          (is (identical? input (.getContent ^Tab (ui/selected-tab (.lookup scene "#left-pane .tab-pane")))))
          (is (= "Retained" (.getText input)))
          (testing "Each region toggle saves immediately without closing the window"
            (doseq [[region command] [[:left :window.toggle-left-pane]
                                      [:right :window.toggle-right-pane]
                                      [:bottom :window.toggle-bottom-pane]]]
              (test-util/handler-run command contexts nil)
              (is (= #{region} (prefs/get prefs [:window :hidden-panes])))
              (test-util/handler-run command contexts nil)
              (is (= #{} (prefs/get prefs [:window :hidden-panes])))))
          (finally
            (ui/advance-ui-user-data-component! scene :editor.app-view/ui nil)))
        (is (= 1 @console-disposals))))))

;; Whole-region hiding retains mounted content; individual hiding unmounts it.
;; Legacy Changed Files visibility moves to the nested per-pane preference.
(deftest initially-hidden-pane-content-test
  (test-util/with-loaded-project
    (ui/run-now
      (let [prefs (test-util/make-test-prefs)
            root (ui/load-fxml "editor.fxml")
            scene (Scene. root)
            stage (doto (Stage.) (.setScene scene))
            described-count (atom 0)
            disposed-count (atom 0)

            console-mounted
            (fn console-mounted []
              (fn dispose-console [] (swap! disposed-count inc)))

            definition
            {:id :console
             :title "Console"
             :location :bottom
             :content (fn describe-console [_]
                        (swap! described-count inc)
                        {:fx/type fx/ext-effect
                         :fn console-mounted
                         :desc {:fx/type fx.label/lifecycle :text "Console"}})}]
        (.applyCss root)
        (.layout root)
        (prefs/set! prefs [:window :hidden-panes] #{:bottom :changed-files})
        (g/transact (g/set-property app-view :scene scene))
        (ui/context! root :global {:app-view app-view :prefs prefs} nil)
        (app-view/set-built-in-panes! app-view [definition])
        (app-view/initialize-panes! app-view scene prefs test-util/localization)
        (try
          (is (= #{:bottom} (prefs/get prefs [:window :hidden-panes])))
          (is (= #{:changed-files} (prefs/get prefs [:window :layout :hidden-panes])))
          (is (nil? (.lookup scene "#bottom-pane")))
          (is (= 1 @described-count))
          (is (zero? @disposed-count))
          (app-view/show-pane! scene :console)
          (is (= 2 @described-count))
          (is (.lookup scene "#bottom-pane"))
          (test-util/handler-run :window.toggle-bottom-pane
                                 [(handler/->context :global {:main-stage stage
                                                              :app-view app-view
                                                              :prefs prefs})]
                                 nil)
          (is (zero? @disposed-count))
          (test-util/handler-run :window.toggle-pane
                                 [(handler/->context :global {:main-stage stage
                                                              :app-view app-view
                                                              :prefs prefs})]
                                 :console)
          (is (= 1 @disposed-count))
          (app-view/show-pane! scene :console)
          (is (.lookup scene "#bottom-pane"))
          (is (= 4 @described-count))
          (finally
            (ui/advance-ui-user-data-component! scene :editor.app-view/ui nil)))
        (is (= 2 @disposed-count))))))

;; Ratios restore after mounting and layout, save on divider release, and
;; survive hiding groups without saving temporary positions on window close.
(deftest restored-pane-split-positions-test
  (test-util/with-loaded-project
    (ui/run-now
      (let [prefs (test-util/make-test-prefs)
            root (ui/load-fxml "editor.fxml")
            scene (Scene. root)

            stage
            (doto (Stage.)
              (.setScene scene)
              (.setX 0.0)
              (.setY 0.0)
              (.setWidth 1200.0)
              (.setHeight 900.0))

            definitions
            (mapv (fn [[id location]]
                    {:id id
                     :title (name id)
                     :location location
                     :content (constantly {:fx/type fx.label/lifecycle :text (name id)})})
                  [[:assets :left-top] [:changed-files :left-bottom]
                   [:outline :right-top] [:properties :right-bottom]
                   [:console :bottom]])

            contexts
            [(handler/->context :global {:main-stage stage :app-view app-view :prefs prefs})]]
        (try
          (.resize root 1200.0 900.0)
          (.applyCss root)
          (.layout root)
          (prefs/set! prefs [:window :hidden-panes] #{:bottom})
          (prefs/set! prefs [:window :split-positions]
                      {:assets-split [0.575] :right-split [0.4] :center-split [0.72]})
          (g/transact (g/set-property app-view :scene scene))
          (ui/context! root :global {:app-view app-view :prefs prefs} nil)
          (app-view/set-built-in-panes! app-view definitions)
          (app-view/initialize-panes! app-view scene prefs test-util/localization)
          (app-view/restore-split-positions! scene prefs)
          (.layout root)
          (doseq [[id expected] [["assets-split" 0.575] ["right-split" 0.4]]]
            (let [split ^SplitPane (.lookup root (str "#" id))
                  actual (aget (.getDividerPositions split) 0)]
              (is (< (Math/abs (- (double expected) actual)) 0.01))))
          (testing "Revealing a region that started hidden restores its saved ratio"
            (is (nil? (.lookup root "#bottom-pane")))
            (.resize root 1200.0 720.0)
            (.layout root)
            (test-util/handler-run :window.toggle-bottom-pane contexts nil)
            (.layout root)
            (let [^SplitPane center-split (.lookup root "#center-split")]
              (is (< (Math/abs (- 0.72 (aget (.getDividerPositions center-split) 0))) 0.01))))
          (testing "Divider changes save only their split on release"
            (doseq [id [:assets-split :right-split :main-split :workbench-split :center-split]]
              (let [^SplitPane split
                    (get (g/node-value app-view :pane-controls) id)

                    divider
                    (coll/first-where #(identical? split (ui/closest-node-of-type SplitPane %))
                                      (.lookupAll split ".split-pane-divider"))

                    before (prefs/get prefs [:window :split-positions])
                    position-before (first (.getDividerPositions split))]
                (.setDividerPosition split 0 (+ (double position-before) 0.05))
                (.layout root)
                (is (= before (prefs/get prefs [:window :split-positions])))
                (Event/fireEvent divider
                                 (MouseEvent. MouseEvent/MOUSE_RELEASED
                                              0.0 0.0 0.0 0.0 MouseButton/PRIMARY 1
                                              false false false false
                                              false false false false false true nil))
                (let [positions (vec (.getDividerPositions split))]
                  (is (not= position-before (first positions)))
                  (is (= (assoc before id positions)
                         (prefs/get prefs [:window :split-positions]))))
                (.layout root))))
          (testing "Hiding a group and closing preserves the last dragged ratio"
            (let [positions (prefs/get prefs [:window :split-positions])

                  ^SplitPane left
                  (:assets-split (g/node-value app-view :pane-controls))]
              (test-util/handler-run :window.toggle-pane contexts :changed-files)
              (.layout root)
              (is (= 1 (count (.getItems left))))
              (app-view/store-window-dimensions! stage prefs)
              (is (= positions (prefs/get prefs [:window :split-positions])))
              (app-view/show-pane! scene :changed-files)
              (.layout root)
              (is (< (Math/abs (- (double (first (:assets-split positions)))
                                  (aget (.getDividerPositions left) 0)))
                     0.01))))
          (testing "A region retains its divider ratio when resized while hidden"
            (let [^SplitPane split (.lookup root "#center-split")
                  ratio-before (aget (.getDividerPositions split) 0)]
              (test-util/handler-run :window.toggle-bottom-pane contexts nil)
              (.resize root 1200.0 900.0)
              (.layout root)
              (test-util/handler-run :window.toggle-bottom-pane contexts nil)
              (.layout root)
              (is (< (Math/abs (- ratio-before (aget (.getDividerPositions split) 0))) 0.01))))
          (finally
            (ui/advance-ui-user-data-component! scene :editor.app-view/ui nil)
            (.close stage)))))))

;; Outline keeps per-resource expansion state while returning nil. Resource
;; closure while hidden must still discard the closed resource's local state.
(deftest outline-resource-transition-test
  (ui/run-now
    (let [tree (TreeView.)

          describe
          (fn describe [id active open]
            {:fx/type outline-view/outline-tree-view
             :tree-view tree
             :localization test-util/localization
             :active-resource-node id
             :open-resource-nodes open
             :selection []
             :active-outline (when active
                               {:node-id id
                                :label (str "Resource " id)
                                :children [{:node-id (* 10 id)
                                            :label "First branch"
                                            :children [{:node-id (* 100 id) :label "Leaf"}]}
                                           {:node-id (inc (* 10 id))
                                            :label "Second branch"
                                            :children [{:node-id (inc (* 100 id)) :label "Leaf"}]}]})
             :hidden-node-outline-key-paths #{}
             :outline-name-paths #{}})

          branch
          (fn branch [index] (.get (.getChildren (.getRoot tree)) (int index)))

          component (volatile! (fx/create-component (describe 1 true [1 2 3])))]
      (try
        (is (= 1 (:node-id (.getValue (.getRoot tree)))))
        (.setExpanded ^TreeItem (branch 0) true)
        (vswap! component fx/advance-component (describe 2 true [1 2 3]))
        (is (= 2 (:node-id (.getValue (.getRoot tree)))))
        (is (not (.isExpanded ^TreeItem (branch 0))))
        (.setExpanded ^TreeItem (branch 1) true)
        (vswap! component fx/advance-component (describe 3 false [1 2 3]))
        (is (nil? (fx/instance @component)))
        (vswap! component fx/advance-component (describe 1 true [1 2 3]))
        (is (identical? tree (fx/instance @component)))
        (is (.isExpanded ^TreeItem (branch 0)))
        (is (not (.isExpanded ^TreeItem (branch 1))))
        (vswap! component fx/advance-component (describe 3 false [2 3]))
        (is (nil? (fx/instance @component)))
        (vswap! component fx/advance-component (describe 2 true [2 3]))
        (is (not (.isExpanded ^TreeItem (branch 0))))
        (is (.isExpanded ^TreeItem (branch 1)))
        (vswap! component fx/advance-component (describe 1 true [1 2 3]))
        (is (not (.isExpanded ^TreeItem (branch 0))))
        (finally
          (fx/delete-component @component))))))

;; A nonnil content description can render nil without losing its local state.
;; Its tab and empty location disappear and reappear with the same pane.
(deftest rendered-nil-pane-test
  (ui/run-now
    (let [prefs (test-util/make-test-prefs)
          left (SplitPane.)
          right (SplitPane.)
          bottom (AnchorPane.)

          content
          (fn content [{:keys [visible state swap-state]}]
            (when visible
              {:fx/type fx.button/lifecycle
               :text (str state)
               :on-action (fn increment-count [_] (swap-state inc))}))

          describe
          (fn describe [visible mounted]
            {:fx/type panes/layout
             :panes [{:id :outline
                      :title "Outline"
                      :location :right-top
                      :content-desc (when mounted
                                      {:fx/type fx/ext-state
                                       :initial-state 42
                                       :desc {:fx/type content :visible visible}})}]
             :hidden #{}
             :localization test-util/localization
             :prefs prefs
             :assets-split left
             :right-split right
             :bottom-container bottom})

          watches-before (count (.getWatches ^IRef test-util/localization))
          component (volatile! (fx/create-component (describe false true)))]
      (try
        (is (zero? (count (.getItems right))))
        (vswap! component fx/advance-component (describe true true))
        (let [host ^TabPane (first (.getItems right))
              tab ^Tab (first (.getTabs host))]
          (is (= "42" (.getText ^Button (.getContent tab))))
          (.fire ^Button (.getContent tab))
          (vswap! component fx/advance-component (describe false true))
          (is (zero? (count (.getItems right))))
          (is (zero? (count (.getTabs host))))
          (vswap! component fx/advance-component (describe true true))
          (is (not (identical? tab (first (.getTabs host)))))
          (is (= "43" (.getText ^Button (.getContent ^Tab (first (.getTabs host))))))
          (vswap! component fx/advance-component (describe false false))
          (is (zero? (count (.getItems right))))
          (vswap! component fx/advance-component (describe true true))
          (is (= "42" (.getText ^Button (.getContent ^Tab (first (.getTabs host)))))))
        (finally
          (fx/delete-component @component)))
      (is (= watches-before (count (.getWatches ^IRef test-util/localization)))))))
