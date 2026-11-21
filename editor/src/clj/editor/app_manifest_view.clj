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

(ns editor.app-manifest-view
  (:require [cljfx.api :as fx]
            [cljfx.fx.column-constraints :as fx.column-constraints]
            [cljfx.fx.custom-menu-item :as fx.custom-menu-item]
            [cljfx.fx.menu :as fx.menu]
            [cljfx.fx.menu-item :as fx.menu-item]
            [cljfx.fx.region :as fx.region]
            [cljfx.fx.separator-menu-item :as fx.separator-menu-item]
            [cljfx.fx.stack-pane :as fx.stack-pane]
            [cljfx.lifecycle :as fx.lifecycle]
            [cljfx.mutator :as fx.mutator]
            [clojure.set :as set]
            [dynamo.graph :as g]
            [editor.app-manifest :as app-manifest]
            [editor.dialogs :as dialogs]
            [editor.editor-extensions.node-types :as node-types]
            [editor.fxui :as fxui]
            [editor.localization :as localization]
            [editor.resource :as resource]
            [editor.ui :as ui]
            [editor.view :as view]
            [editor.workspace :as workspace]
            [util.coll :as coll]
            [util.eduction :as e]
            [util.fn :as fn])
  (:import [javafx.beans.value ChangeListener ObservableValue]
           [javafx.event ActionEvent]
           [javafx.scene Node]
           [javafx.scene.control CheckBox MenuItem ScrollPane]
           [javafx.scene.layout GridPane VBox]))

(set! *warn-on-reflection* true)

(def ^:private accessible-text
  (fx/make-prop (fx.mutator/setter (fn set-accessible-text [^Node node text]
                                     (.setAccessibleText node text)))
                fx.lifecycle/scalar))

(def ^:private platform-groups
  [["macOS" [[:arm64-osx "arm64"] [:x86_64-osx "x86_64"]]]
   ["Windows" [[:x86_64-win32 "x86_64"]]]
   ["Linux" [[:x86_64-linux "x86_64"] [:arm64-linux "arm64"]]]
   ["iOS" [[:arm64-ios "arm64"] [:arm64_sim-ios (localization/message "form.app-manifest.ios-simulator")]]]
   ["Android" [[:arm64-android "arm64"] [:armv7-android "armv7"] [:x86_64-android "x86_64"]]]
   ["HTML5" [[:wasm-web "wasm"] [:wasm_pthread-web (localization/message "form.app-manifest.wasm-threads")]]]])

(def ^:private platforms (mapv first platform-groups))

(def ^:private all-platforms (set platforms))

(def ^:private desktop-platforms #{"macOS" "Windows" "Linux"})

(def ^:private variants
  [[:release (localization/message "dialog.bundle.option.variant.release")]
   [:debug (localization/message "dialog.bundle.option.variant.debug")]
   [:headless (localization/message "dialog.bundle.option.variant.headless")]])

(def ^:private all-variants (coll/into-> variants #{} (map first)))

(def ^:private graphics-matrix
  {:title (localization/message "form.app-manifest.graphics-backends")
   :rows [{:id :opengl
           :label "OpenGL / OpenGL ES"
           :architectures (disj (set/difference app-manifest/all-platforms app-manifest/web) :arm64_sim-ios)
           :selected #{"Windows" "Linux" "iOS" "Android"}}
          {:id :vulkan
           :label "Vulkan"
           :architectures (set/union app-manifest/vulkan app-manifest/vulkan-osx)
           :selected #{"macOS" "Android"}}
          {:id :metal
           :label "Metal"
           :architectures (set/union app-manifest/macos app-manifest/metal-ios)
           :fixed-architectures #{:arm64_sim-ios}}
          {:id :dx12 :label "DirectX 12" :architectures app-manifest/windows}
          {:id :webgl :label "WebGL" :architectures app-manifest/web :selected #{"HTML5"}}
          {:id :webgpu :label "WebGPU" :architectures app-manifest/web}]})

(def ^:private feature-matrices
  [{:title (localization/message "form.group.components")
    :rows [{:id :sound :label (localization/message "resource.type.sound") :default-on true}
           {:id :wav :label "WAV" :parent :sound :default-on true}
           {:id :ogg :label "OGG" :parent :sound :default-on true}
           {:id :opus :label "OPUS" :parent :sound :default-on true :default-variants #{:debug}}
           {:id :gui :label (localization/message "resource.type.gui") :default-on true}
           {:id :tilemap :label (localization/message "form.label.project.tilemap") :default-on true}
           {:id :label :label (localization/message "resource.type.label") :default-on true}
           {:id :particlefx :label (localization/message "resource.type.particlefx") :default-on true}
           {:id :rig :label (localization/message "form.app-manifest.rig") :default-on true}
           {:id :model :label (localization/message "resource.type.model") :parent :rig :default-on true}]}

   {:title (localization/message "form.group.runtime")
    :rows [{:id :physics :label (localization/message "form.label.project.physics") :default-on true}
           {:id :box2d-v2 :label "Box2D v2" :parent :physics :alternative :box2d-v3}
           {:id :script-box2d-v2 :label (localization/message "form.app-manifest.lua-api") :parent :box2d-v2 :default-on true}
           {:id :box2d-v3 :label "Box2D v3" :parent :physics :alternative :box2d-v2 :default-on true}
           {:id :script-box2d-v3 :label (localization/message "form.app-manifest.lua-api") :parent :box2d-v3 :default-on true}
           {:id :bullet :label "Bullet 3D" :parent :physics :default-on true}
           {:id :script-bullet :label (localization/message "form.app-manifest.lua-api") :parent :bullet :default-on true}
           {:id :input :label (localization/message "form.label.project.input") :default-on true}
           {:id :liveupdate :label (localization/message "form.label.project.liveupdate") :default-on true}
           {:id :datastore :label (localization/message "form.app-manifest.datastore") :default-on true}
           {:id :basis-transcoder
            :label (localization/message "form.app-manifest.basis-transcoder")
            :default-on true}
           {:id :android-support :label (localization/message "form.app-manifest.android-support") :supported #{"Android"}}
           {:id :image :label (localization/message "form.app-manifest.image-module") :default-on true}
           {:id :script-types :label (localization/message "form.app-manifest.script-types") :default-on true}
           {:id :font-layout :label (localization/message "form.app-manifest.full-text-layout")}
           {:id :rich-text :label (localization/message "form.app-manifest.rich-text") :default-on true}
           {:id :profiler
            :label (localization/message "property.appmanifest.profiler")
            :default-on true
            :default-variants #{:debug}}
           {:id :profiler-basic
            :label (localization/message "form.app-manifest.profiler-basic")
            :parent :profiler
            :default-on true}
           {:id :profiler-remotery
            :label (localization/message "form.app-manifest.profiler-remotery")
            :parent :profiler
            :supported (disj all-platforms "HTML5")
            :default-on true}
           {:id :profiler-js
            :label (localization/message "form.app-manifest.profiler-js")
            :parent :profiler
            :supported #{"HTML5"}
            :default-on true}
           {:id :record
            :label (localization/message "form.app-manifest.recording")
            :supported desktop-platforms
            :supported-variants #{:release :debug}
            :default-on true
            :default-variants #{:debug}}
           {:id :record-script
            :label (localization/message "form.app-manifest.lua-api")
            :parent :record
            :default-on true}
           {:id :record-service
            :label (localization/message "form.app-manifest.recording-service")
            :parent :record
            :supported-variants #{:debug}
            :default-on true}]}])

(def ^:private sample-selections
  {:visible-columns #{}
   :features
   {:all
    (coll/pair-map-by
      identity
      (fn [variant]
        (coll/into-> feature-matrices {}
          (mapcat :rows)
          (filter (fn [{:keys [default-on default-variants supported-variants]}]
                    (and default-on
                         (or (nil? default-variants) (contains? default-variants variant))
                         (or (nil? supported-variants) (contains? supported-variants variant)))))
          (map (fn [row] [(:id row) true]))))
      all-variants)}})

(defn- set-platform-variants [state id alternative column-id target-variants enabled]
  (reduce (fn [state variant]
            (cond-> (assoc-in state [:features column-id variant id] enabled)
              (and enabled alternative)
              (assoc-in [:features column-id variant alternative] false)))
          state
          target-variants))

(defn- feature-source [state {column-id :id :keys [platform]} variant id]
  (let [features (:features state)]
    (cond
      (contains? (get-in features [column-id variant]) id) column-id
      (contains? (get-in features [platform variant]) id) platform
      :else :all)))

(defn- feature-selected? [state column variant id]
  (get-in state [:features (feature-source state column variant id) variant id] false))

(defn- feature-variant-selection
  [state {:keys [id supported supported-variants prerequisites]} {column-id :id :keys [platform] :as column}]
  (let [available
        (if (if (= :all column-id) (not (coll/empty? supported)) (contains? supported platform))
          (coll/into-> supported-variants #{}
            (filter (fn [variant]
                      (coll/every? (fn [parent-id]
                                     (feature-selected? state column variant parent-id))
                                   prerequisites))))
          #{})]
    {:available available
     :selected (coll/into-> available #{} (filter #(feature-selected? state column % id)))}))

(defn- cell-label [localize feature {:keys [id label]}]
  (if (= :all id)
    (localize (localization/message "form.app-manifest.all-platforms-for-feature" {"feature" feature}))
    (str feature " (" label ")")))

(defn- variant-menu-item
  [localize available selected set-variants [variant label]]
  (let [text (localize label)
        checked (contains? selected variant)]
    {:fx/type fx.custom-menu-item/lifecycle
     :text text
     :disable (not (contains? available variant))
     :hide-on-click false
     :on-action (fn toggle-variant [_event]
                  (set-variants [variant] (not checked)))
     :content {:fx/type fxui/horizontal
               :pick-on-bounds true
               :children [{:fx/type fxui/check-box
                           :text text
                           :mouse-transparent true
                           :focus-traversable false
                           :selected checked}]}}))

(defn- feature-cell
  [localize state swap-state {:keys [id text alternative] :as row} {column-id :id :as column}]
  (let [{:keys [available selected]}
        (feature-variant-selection state row column)

        checked (= all-variants selected)
        mixed (and (not checked) (not (coll/empty? selected)))
        disabled (coll/empty? available)

        set-variants
        (fn set-variants [target-variants enabled]
          (swap-state set-platform-variants id alternative column-id target-variants enabled))

        menu-label
        (localize
          (if (= :all column-id)
            (localization/message "form.app-manifest.all-platforms-variant-checklist" {"feature" text})
            (localization/message "form.app-manifest.variant-checklist"
                                  {"feature" text "platform" (:label column)})))]
    {:fx/type fxui/horizontal
     :alignment :center
     :spacing 3
     :children [{:fx/type fxui/check-box
                 :max-width :use-pref-size
                 accessible-text (cell-label localize text column)
                 :disable disabled
                 :selected checked
                 :indeterminate mixed
                 :on-action (fn toggle-cell [^ActionEvent event]
                              ;; Restore the checkbox state if the next
                              ;; selection also renders as mixed.
                              (doto ^CheckBox (.getSource event)
                                (.setSelected checked)
                                (.setIndeterminate mixed))
                              (set-variants available (not= selected available)))}
                {:fx/type fxui/menu-button
                 :style-class "matrix-variants"
                 accessible-text menu-label
                 :tooltip menu-label
                 :disable (and disabled (not= :all column-id))
                 :items (-> (mapv (fn/partial variant-menu-item localize available selected set-variants) variants)
                            (conj {:fx/type fx.separator-menu-item/lifecycle}
                                  {:fx/type fx.menu-item/lifecycle
                                   :text
                                   (localize (localization/message "form.app-manifest.compare-variants"))
                                   :on-action
                                   (fn open-comparison [^ActionEvent event]
                                     (let [^MenuItem item (.getSource event)]
                                       (swap-state assoc :comparison
                                                   {:row row
                                                    :owner (some-> item .getParentPopup .getOwnerWindow)})))}))}]}))

(defn- inherit-feature-variant [state {:keys [id alternative]} {column-id :id :as column} variant]
  (let [path [:features column-id variant]
        inherited-state (update-in state path dissoc id)]
    ;; Restoring an enabled Box2D version clears a conflicting local version.
    (cond-> inherited-state
      (and alternative
           (feature-selected? inherited-state column variant id)
           (get-in inherited-state (conj path alternative)))
      (update-in path dissoc alternative))))

(defn- comparison-cell
  [localize state swap-state {:keys [id] :as row} {column-id :id :keys [platform] :as column} variant]
  (let [{:keys [available selected]}
        (feature-variant-selection state row column)

        available-for-variant (contains? available variant)
        source (feature-source state column variant id)

        all-label
        (localize (localization/message "form.app-manifest.all-platforms"))

        source-label (if (= :all source) all-label source)
        parent-label (if (= column-id platform) all-label platform)
        override (and (not= :all column-id) (= column-id source))

        status
        (cond
          (not available-for-variant) (localization/message "form.app-manifest.unavailable")
          (contains? selected variant) (localization/message "form.app-manifest.enabled")
          :else (localization/message "form.app-manifest.disabled"))]
    {:fx/type fxui/vertical
     :style-class "matrix-comparison-cell"
     :spacing 3
     :children
     (cond-> [(cond-> {:fx/type fxui/paragraph :text (localize status)}
                (not available-for-variant) (assoc :style-class "matrix-comparison-secondary"))]
       (and (not= :all column-id) (or available-for-variant override))
       (conj {:fx/type fxui/paragraph
              :style-class (if override "matrix-comparison-override" "matrix-comparison-secondary")
              :text (localize (if override
                                (localization/message "form.app-manifest.override")
                                (localization/message "form.app-manifest.inherited-from" {"scope" source-label})))})

       override
       (conj {:fx/type fxui/legacy-button
              :focus-traversable true
              :text (localize (localization/message "form.app-manifest.use-inherited-value" {"scope" parent-label}))
              :on-action (fn use-inherited-value [_event]
                           (swap-state inherit-feature-variant row column variant))}))}))

(defn- comparison-dialog [localize state swap-state columns {:keys [row owner]}]
  (let [title
        (localize (localization/message "form.app-manifest.compare-variants-title" {"feature" (:text row)}))

        close! (fn close-comparison [_event] (swap-state dissoc :comparison))

        heading
        {:fx/type fxui/paragraph :style-class "matrix-comparison-heading"}

        variant-headings
        (coll/into-> variants [(assoc heading :text (localize (localization/message "form.app-manifest.variant")))]
          (map-indexed (fn [i [_ label]]
                         (assoc heading :text (localize label) :grid-pane/row (inc i)))))

        cells
        (coll/into-> columns variant-headings
          (map-indexed
            (fn [i column]
              (let [column-index (inc i)]
                (coll/into-> variants [(assoc heading :text (:label column) :grid-pane/column column-index)]
                  (map-indexed (fn [j [variant _]]
                                 (assoc (comparison-cell localize state swap-state row column variant)
                                   :grid-pane/column column-index
                                   :grid-pane/row (inc j))))))))
          cat)]
    (cond-> {:fx/type dialogs/dialog-stage
             :width (min 760 (max 440 (+ 134 (* 154 (count columns)))))
             :title title
             :showing true
             :resizable true
             :on-close-request close!
             :header {:fx/type fxui/legacy-label :variant :header :text title}
             :content {:fx/type fxui/scroll
                       :fit-to-width false
                       :content {:fx/type fxui/grid
                                 :padding :small
                                 :column-constraints
                                 (coll/into-> columns [{:fx/type fx.column-constraints/lifecycle :min-width 86}]
                                   (map (constantly {:fx/type fx.column-constraints/lifecycle :min-width 154})))
                                 :children cells}}
             :footer {:fx/type dialogs/dialog-buttons
                      :children [{:fx/type fxui/legacy-button
                                  :text (localize (localization/message "dialog.button.close"))
                                  :on-action close!
                                  :cancel-button true}]}}
      owner (assoc :owner owner))))

(defn- extension-cell
  [localize state swap-state {:keys [id text supported default-on]} {column-id :id :keys [platform] :as column}]
  (let [checked
        (get-in state [:extensions id column-id]
                (get-in state [:extensions id platform]
                        (get-in state [:extensions id :all] default-on)))]
    {:fx/type fxui/check-box
     accessible-text (cell-label localize text column)
     :max-width :use-pref-size
     :disable (and (not= :all column-id) (not (contains? supported platform)))
     :selected checked
     :on-action (fn toggle-platform [_event]
                  (swap-state assoc-in [:extensions id column-id] (not checked)))}))

(defn- update-graphics-guide [guide kind cell active]
  (if active
    (cond-> (assoc guide kind cell)
      (= :focused kind) (dissoc :hovered))
    (if (= cell (get guide kind)) (dissoc guide kind) guide)))

(defn- graphics-cell
  [localize state swap-state active-guide swap-guide
   {:keys [id text architectures fixed-architectures selected]}
   {architecture :id :keys [platform] :as column}]
  (let [cell {:row id :column architecture :platform platform}
        name (cell-label localize text column)
        fixed (contains? fixed-architectures architecture)
        supported (contains? architectures architecture)
        path [:graphics id architecture]
        checked (get-in state path (contains? selected platform))

        unavailable
        (localize (localization/message "form.app-manifest.unavailable"))

        fixed-tooltip
        (localize (localization/message "form.app-manifest.fixed-simulator-backend"))]
    {:fx/type fx.stack-pane/lifecycle
     :style-class ["matrix-graphics-cell"]
     :pseudo-classes (if (= architecture (:column active-guide)) #{:guided} #{})
     :pick-on-bounds true
     :on-mouse-entered (fn guide-hovered-cell [_event] (swap-guide update-graphics-guide :hovered cell true))
     :on-mouse-exited (fn unguide-hovered-cell [_event] (swap-guide update-graphics-guide :hovered cell false))
     :children [(cond
                  fixed
                  {:fx/type fxui/paragraph
                   :style-class "matrix-fixed-graphics"
                   :alignment :center
                   :tooltip fixed-tooltip
                   accessible-text fixed-tooltip
                   :graphic {:fx/type fx.stack-pane/lifecycle
                             :pref-width 56
                             :children [{:fx/type fxui/check-box
                                         :style-class "matrix-fixed-checkbox"
                                         :disable true
                                         :mouse-transparent true
                                         :max-width :use-pref-size
                                         :selected true}
                                        {:fx/type fxui/icon-graphic
                                         :type :icon/lock
                                         :size 10
                                         :stack-pane/alignment :center-right
                                         :stack-pane/margin {:right 4}}]}}

                  supported
                  {:fx/type fxui/check-box
                   accessible-text name
                   :max-width :use-pref-size
                   :selected checked
                   :on-focused-changed (fn guide-focused-cell [focused]
                                         (swap-guide update-graphics-guide :focused cell focused))
                   :on-action (fn toggle-backend [_event]
                                (swap-state assoc-in path (not checked)))}

                  :else
                  {:fx/type fxui/paragraph
                   :alignment :center
                   :color :hint
                   :text "·"
                   accessible-text (localize (localization/message "form.app-manifest.unavailable-cell"
                                                                   {"setting" name "state" unavailable}))})]}))

(defn- graphics-layout [groups]
  (let [label-width 176

        architecture-widths
        (coll/into-> groups []
          (mapcat (fn [{:keys [columns]}]
                    (let [width (if (= 1 (count columns)) 72 56)]
                      (mapv (constantly width) columns)))))]
    {:width (reduce + label-width architecture-widths)
     :groups groups
     :constraints
     (coll/into-> architecture-widths [{:fx/type fx.column-constraints/lifecycle :pref-width label-width}]
       (map (fn [width]
              {:fx/type fx.column-constraints/lifecycle :pref-width width :halignment :center})))}))

(defn- feature-layout [columns]
  {:width (+ 224 64 (* 124 (dec (count columns))))
   :constraints
   (coll/into-> columns [{:fx/type fx.column-constraints/lifecycle :pref-width 224}]
     (map (fn [{:keys [id]}]
            {:fx/type fx.column-constraints/lifecycle
             :pref-width (if (= :all id) 64 124)
             :halignment :center})))})

(defn- matrix-row [constraints index label cells guided]
  {:fx/type fxui/grid
   :style-class (if (zero? index) "matrix-header" "matrix-row")
   :view-order (if (zero? index) -1 0)
   :pseudo-classes (cond-> #{}
                     (odd? index) (conj :odd)
                     guided (conj :guided))
   :column-constraints constraints
   :children (coll/into-> cells [label]
               (map-indexed (fn [i cell] (assoc cell :grid-pane/column (inc i)))))})

(defn- parent-rows [row-by-id row]
  (loop [rows []
         parent (:parent row)]
    (if-let [parent-row (row-by-id parent)]
      (recur (conj rows parent-row) (:parent parent-row))
      rows)))

(defn- feature-row
  [localize render-cell columns constraints row-by-id active-guide index {:keys [id label parent] :as row}]
  (let [text (localize label)
        parents (parent-rows row-by-id row)
        lineage (conj parents row)

        control-name
        (if parent
          (localize (localization/message "form.app-manifest.child-feature"
                                          {"parent" (localize (:label (row-by-id parent)))
                                           "feature" text}))
          text)

        cell
        (assoc row
          :text control-name
          :supported (reduce set/intersection all-platforms
                             (e/keep :supported lineage))
          :supported-variants (reduce set/intersection all-variants
                                      (e/keep :supported-variants lineage))
          :prerequisites (mapv :id parents))]
    (matrix-row
      constraints
      (inc index)
      {:fx/type fxui/paragraph
       :style-class "matrix-row-label"
       :view-order -1
       :padding {:left (+ 8 (* 20 (count parents)))}
       :alignment :center-left
       :text text}
      (e/map (fn/partial render-cell cell) columns)
      (= id (:row active-guide)))))

(defn- platform-headings [groups active-guide]
  (second
    (reduce (fn [[column headings] {:keys [label columns]}]
              (let [span (count columns)]
                [(+ column span)
                 (conj headings {:fx/type fxui/paragraph
                                 :style-class "matrix-platform-heading"
                                 :pseudo-classes (if (= label (:platform active-guide)) #{:guided} #{})
                                 :grid-pane/column column
                                 :grid-pane/column-span span
                                 :alignment :center
                                 :text label})]))
            [1 []]
            groups)))

(defn- matrix [localize render-cell columns {:keys [width constraints groups active-guide]} {:keys [title rows empty-label]}]
  (let [group-headings (when groups (platform-headings groups active-guide))

        headings
        (matrix-row
          constraints
          0
          (cond-> {:fx/type fxui/paragraph
                   :padding {:left 8}
                   :alignment :center-left
                   :style-class ["cljfx-form-group-title" "matrix-row-label"]
                   :view-order -1
                   :text (localize title)}
            groups (assoc :grid-pane/row-span 2))
          (e/map (fn [{:keys [id label menu-label]}]
                   (cond-> {:fx/type fxui/paragraph :text label :alignment :center}
                     groups (assoc :text menu-label
                                   :style-class "matrix-architecture-heading"
                                   :pseudo-classes (if (= id (:column active-guide)) #{:guided} #{})
                                   :text-alignment :center
                                   :grid-pane/row 1)))
                 columns)
          false)

        header
        (update headings :children into group-headings)

        row-by-id (coll/pair-map-by :id identity rows)

        table
        {:fx/type fxui/vertical
         :style-class (cond-> ["app-manifest-matrix"]
                        (not groups) (conj "app-manifest-feature-matrix"))
         :max-width width
         :children (if (and empty-label (coll/empty? rows))
                     [header {:fx/type fxui/paragraph
                              :padding {:top 6 :left 8 :bottom 6}
                              :text (localize empty-label)}]
                     (coll/into-> rows [header]
                       (map-indexed (fn/partial feature-row localize render-cell columns constraints row-by-id active-guide))))}]
    (if-not group-headings
      table
      {:fx/type fx.stack-pane/lifecycle
       :style-class ["app-manifest-graphics"]
       :max-width width
       :children [table
                  {:fx/type fxui/grid
                   :column-constraints constraints
                   :mouse-transparent true
                   :children (coll/into-> group-headings []
                               (drop 1)
                               (map (fn [{:grid-pane/keys [column]}]
                                      {:fx/type fx.region/lifecycle
                                       :style-class ["matrix-platform-separator"]
                                       :grid-pane/column column
                                       :grid-pane/halignment :left
                                       :grid-pane/vgrow :always
                                       :pref-width 1
                                       :max-width 1})))}]})))

(defn- platform-options [localize]
  (mapv
    (fn [[platform architectures]]
      {:label platform
       :columns
       (coll/into-> architectures
         [{:id platform
           :platform platform
           :label platform
           :menu-label
           (localize (localization/message "form.app-manifest.platform-group" {"platform" platform}))}]
         (map (fn [[id architecture]]
                (let [label (localize architecture)]
                  {:id id
                   :platform platform
                   :label (str platform " " label)
                   :menu-label label}))))})
    platform-groups))

(defn- platform-option [visible-columns swap-state {:keys [id menu-label]}]
  (let [visible (contains? visible-columns id)]
    {:fx/type fx.custom-menu-item/lifecycle
     :text menu-label
     :hide-on-click false
     :on-action (fn toggle-column [_event]
                  (swap-state update :visible-columns (if visible disj conj) id))
     :content {:fx/type fxui/horizontal
               :pick-on-bounds true
               :children [{:fx/type fxui/check-box
                           :text menu-label
                           :mouse-transparent true
                           :focus-traversable false
                           :selected visible}]}}))

(defn- confirm-reset! [localize swap-state ^ActionEvent event]
  (when (dialogs/make-confirmation-dialog
          localize
          {:owner (.. ^Node (.getSource event) getScene getWindow)
           :title (localization/message "form.app-manifest.reset-defaults")
           :header (localization/message "form.app-manifest.reset-confirmation")
           :content {:fx/type fxui/paragraph
                     :style-class "dialog-content-padding"
                     :text (localize (localization/message "form.app-manifest.reset-description"))}
           :icon :icon/circle-question
           :buttons [{:text (localization/message "dialog.button.cancel")
                      :cancel-button true
                      :default-button true
                      :result false}
                     {:text (localization/message "form.app-manifest.reset-confirm")
                      :variant :danger
                      :result true}]})
    (swap-state (constantly sample-selections))))

(defn- preview-controls [localize options visible-columns swap-state]
  {:fx/type fxui/horizontal
   :spacing :small
   :alignment :center-left
   :children [{:fx/type fxui/menu-button
               :text (localize (localization/message "form.app-manifest.platforms"))
               :tooltip (localize (localization/message "form.app-manifest.platforms-help"))
               :items (mapv (fn [{:keys [label columns]}]
                              {:fx/type fx.menu/lifecycle
                               :text label
                               :items (mapv (fn/partial platform-option visible-columns swap-state) columns)})
                            options)}
              {:fx/type fxui/button
               :text (localize (localization/message "form.app-manifest.reset-defaults"))
               :focus-traversable true
               :on-action (fn/partial confirm-reset! localize swap-state)}]})

(defn- update-pinned-matrices! [^ScrollPane scroll]
  (when-let [^VBox content (.getContent scroll)]
    (let [bounds (.getLayoutBounds content)
          viewport (.getViewportBounds scroll)

          scroll-x
          (Math/round (* (.getHvalue scroll) (max 0.0 (- (.getWidth bounds) (.getWidth viewport)))))

          scroll-y
          (Math/round (* (.getVvalue scroll) (max 0.0 (- (.getHeight bounds) (.getHeight viewport)))))]
      (doseq [^VBox table (.getChildren content)
              :when (.contains (.getStyleClass table) "app-manifest-feature-matrix")]
        (let [rows (.getChildren table)
              ^GridPane header (.get rows 0)
              scrolled-x (max 0.0 (- scroll-x (.getLayoutX table)))

              x
              (min scrolled-x (max 0.0 (- (.getWidth table) (.getWidth viewport))))

              y
              (min (max 0.0 (- scroll-y (.getLayoutY table)))
                   (max 0.0 (- (.getHeight table) (.getHeight header))))]
          ;; Move the original nodes to preserve focus and input handling.
          (.setTranslateX table (- scrolled-x x))
          (.setTranslateY header y)
          (doseq [row rows
                  :when (instance? GridPane row)]
            (let [^Node label (.get (.getChildren ^GridPane row) 0)]
              (.setTranslateX label x))))))))

(def ^:private pinned-matrices
  (fx/make-binding-prop
    (fn bind-pinned-matrices [^ScrollPane scroll _]
      (let [properties
            [(.hvalueProperty scroll) (.vvalueProperty scroll) (.viewportBoundsProperty scroll)]

            ^ChangeListener listener
            (fn [_observable _old _new] (update-pinned-matrices! scroll))]
        (doseq [^ObservableValue property properties]
          (.addListener property listener))
        #(doseq [^ObservableValue property properties] (.removeListener property listener))))
    fx.lifecycle/scalar))

(ui/defc ^:private graphics-table
  {:compose [{:fx/type fx/ext-state :initial-state {}}]}
  [{:keys [state swap-state localize preview-state swap-preview-state columns layout]}]
  (let [guide (or (:hovered state) (:focused state))]
    {:fx/type fxui/vertical
     :spacing 5
     :children [(matrix localize
                        (fn/partial graphics-cell localize preview-state swap-preview-state guide swap-state)
                        columns
                        (assoc layout :active-guide guide)
                        graphics-matrix)
                {:fx/type fxui/paragraph
                 :style-class "matrix-graphics-legend"
                 :color :hint
                 :v-box/margin {:left 8}
                 :text (localize (localization/message "form.app-manifest.unavailable-legend"))}]}))

(ui/defc ^:private features
  {:compose [{:fx/type fx/ext-state :initial-state sample-selections}
             {:fx/type fx/ext-watcher :ref (:localization props) :key :localization-state}]}
  [{:keys [state swap-state localization-state native-extension-rows]}]
  (let [header
        {:fx/type fxui/vertical
         :spacing :small
         :children [{:fx/type fxui/label
                     :style-class "cljfx-form-title"
                     :text (localization-state (localization/message "form.app-manifest.title"))}
                    {:fx/type fxui/paragraph
                     :text (localization-state
                             (localization/message "form.app-manifest.preview"
                                                   {"openAs" (localization/message "command.file.open-as")
                                                    "code" (localization/message "resource.view.code")}))}]}

        visible-columns (:visible-columns state)
        options (platform-options localization-state)

        graphics-groups (mapv #(update % :columns subvec 1) options)
        graphics-columns (coll/into-> graphics-groups [] (mapcat :columns))
        graphics-layout (graphics-layout graphics-groups)

        columns
        (coll/into-> options [{:id :all :label (localization-state (localization/message "form.app-manifest.all-platforms"))}]
          (mapcat :columns)
          (filter #(contains? visible-columns (:id %))))

        layout (feature-layout columns)
        content-width (+ 48 (max (:width graphics-layout) (:width layout)))

        graphics
        {:fx/type graphics-table
         :localize localization-state
         :preview-state state
         :swap-preview-state swap-state
         :columns graphics-columns
         :layout graphics-layout}

        sections
        (coll/into-> feature-matrices [header graphics
                                       (preview-controls localization-state options visible-columns swap-state)]
          (map (fn/partial matrix
                           localization-state
                           (fn/partial feature-cell localization-state state swap-state)
                           columns
                           layout)))

        extensions
        (matrix
          localization-state
          (fn/partial extension-cell localization-state state swap-state)
          columns
          layout
          {:title (localization/message "form.app-manifest.native-extensions")
           :empty-label (localization/message "form.app-manifest.native-extensions-empty")
           :rows native-extension-rows})]
    {:fx/type fx/ext-let-refs
     :refs (when-let [comparison (:comparison state)]
             {::comparison (comparison-dialog localization-state state swap-state columns comparison)})
     :desc {:fx/type fxui/scroll
            :id "scroll-pane"
            pinned-matrices true
            :content {:fx/type fxui/vertical
                      :padding :medium
                      :spacing 20
                      :min-width (max 560 content-width)
                      :max-width (max 920 content-width)
                      :children (conj sections extensions)}}}))

(g/defnk produce-native-extension-rows [project-settings resource-snapshot resources]
  (let [dependencies (set (get project-settings ["project" "dependencies"]))
        status-map (:status-map resource-snapshot)]
    (coll/into-> resources []
      (filter #(and (= "ext.manifest" (resource/resource-name %)) (= :file (resource/source-type %))))
      (keep (fn [manifest]
              (let [proj-path (resource/proj-path manifest)
                    dependency (get-in status-map [proj-path :library])]
                (when (contains? dependencies dependency)
                  {:id [dependency proj-path]
                   :label (or (resource/parent-proj-path (resource/path manifest))
                              (resource/resource-name manifest))
                   :default-on true})))))))

(g/defnode AppManifestView
  (inherits view/WorkbenchView)
  (input project-settings g/Any)
  (input resource-snapshot g/Any)
  (input resources g/Any)
  (output native-extension-rows g/Any :cached produce-native-extension-rows)
  (output sidebar-panes g/Any (g/constantly [:outline-pane])))

(node-types/register-node-type-name! AppManifestView "app-manifest")

(defn- make-view [parent _resource-node {:keys [localization project tab workspace]}]
  (let [view-node
        (first
          (g/tx-nodes-added
            (g/transact
              {:undoable false}
              (g/make-nodes [view AppManifestView]
                (g/connect project :settings view :project-settings)
                (g/connect workspace :resource-snapshot view :resource-snapshot)
                (g/connect workspace :resource-list view :resources)))))

        displayed-rows (volatile! nil)

        refresh!
        (fn refresh! [_elapsed-time]
          (let [rows (g/node-value view-node :native-extension-rows)]
            (when-not (= @displayed-rows rows)
              (ui/advance-graph-user-data-component!
                view-node :view
                {:fx/type fxui/ext-with-anchor-pane-props
                 :desc {:fx/type ui/ext-value :value parent}
                 :props {:children [{:fx/type features
                                     :anchor-pane/top 0
                                     :anchor-pane/right 0
                                     :anchor-pane/bottom 0
                                     :anchor-pane/left 0
                                     :localization localization
                                     :native-extension-rows rows}]}})
              (vreset! displayed-rows rows))))

        dispose-timer!
        (ui/node-timer! parent 30 "refresh-app-manifest" refresh!)]
    (refresh! 0)
    (ui/on-closed! tab (fn dispose-preview [_event]
                         (dispose-timer!)
                         (ui/advance-graph-user-data-component! view-node :view nil)))
    view-node))

(defn register-view-types [workspace]
  (workspace/register-view-type workspace
    :id :app-manifest
    :label (localization/message "resource.view.app-manifest")
    :make-view-fn #'make-view))
