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
            [cljfx.fx.menu :as fx.menu]
            [cljfx.fx.menu-item :as fx.menu-item]
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
           [javafx.scene.control ButtonBase CheckBox MenuItem ScrollPane]
           [javafx.scene.input MouseButton MouseEvent]
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
  [{:id :components
    :title (localization/message "form.group.components")
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

   {:id :runtime
    :title (localization/message "form.group.runtime")
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

(defn- default-feature-value [{:keys [default-on default-variants]} variant]
  (and (boolean default-on)
       (or (nil? default-variants) (contains? default-variants variant))))

(defn- feature-value [settings {column-id :id :keys [platform]} {:keys [id] :as row} variant]
  (get-in settings [column-id id variant]
          (get-in settings [platform id variant]
                  (get-in settings [:all id variant] (default-feature-value row variant)))))

(defn- set-feature-value [settings {column-id :id :keys [platform]} {:keys [id] :as row} variant enabled]
  (let [inherited
        (if (= :all column-id)
          (default-feature-value row variant)
          (feature-value settings {:id (if (= column-id platform) :all platform)} row variant))]
    (coll/removing-assoc-in settings [column-id id variant] (when (not= enabled inherited) enabled))))

(defn- set-platform-variants [settings {:keys [id alternative-row] :as row} column target-variants enabled]
  (reduce (fn [settings variant]
            (let [updated
                  (set-feature-value settings column row variant enabled)

                  column-id (:id column)]
              (cond
                (or (not enabled) (nil? alternative-row)) updated

                ;; Keep the alternative off even after a parent edit.
                (get-in updated [column-id id variant])
                (assoc-in updated [column-id (:id alternative-row) variant] false)

                :else
                (set-feature-value updated column alternative-row variant false))))
          settings
          target-variants))

(defn- supported-column? [supported {column-id :id :keys [platform]}]
  (if (= :all column-id)
    (not (coll/empty? supported))
    (contains? supported platform)))

(defn- feature-variant-selection
  [settings {:keys [supported supported-variants prerequisites] :as row} column target-variants]
  (let [available
        (if (supported-column? supported column)
          (coll/into-> target-variants #{}
            (filter (fn [variant]
                      (and (contains? supported-variants variant)
                           (coll/every? (fn [parent] (feature-value settings column parent variant))
                                        prerequisites)))))
          #{})]
    {:available available :selected (filterv #(feature-value settings column row %) available)}))

(defn- cell-label [localize feature {:keys [id label]}]
  (if (= :all id)
    (localize (localization/message "form.app-manifest.all-platforms-for-feature" {"feature" feature}))
    (str feature " (" label ")")))

(defn- feature-cell [localize settings update-settings! {:keys [text] :as row} target-variants column]
  (let [{:keys [available selected]}
        (feature-variant-selection settings row column target-variants)

        checked (= (count target-variants) (count selected))
        mixed (and (not checked) (not (coll/empty? selected)))
        disabled (coll/empty? available)]
    {:fx/type fxui/check-box
     :max-width :use-pref-size
     accessible-text (cell-label localize text column)
     :disable disabled
     :selected checked
     :indeterminate mixed
     :on-action (fn toggle-cell [^ActionEvent event]
                  ;; Restore the control state if the selection stays mixed.
                  (doto ^CheckBox (.getSource event)
                    (.setSelected checked)
                    (.setIndeterminate mixed))
                  (update-settings! #(set-platform-variants % row column available
                                                            (not= (count selected) (count available)))))}))

(defn- update-cell-guide [guide kind cell active]
  (if active
    (cond-> (assoc guide kind cell)
      (= :focused kind) (dissoc :hovered))
    (if (= cell (get guide kind)) (dissoc guide kind) guide)))

(defn- track-cell-guide! [table active-table swap-preview-state swap-guide kind cell active]
  (when (and active (not= table active-table))
    (swap-preview-state assoc :guide-table table))
  (swap-guide update-cell-guide kind cell active))

(defn- graphics-cell
  [localize state swap-state active-guide track-guide!
   {:keys [id label architectures fixed-architectures selected]}
   {column-id :id :keys [platform architecture-ids] :as column}]
  (let [cell {:row id :column column-id :platform platform}
        name (cell-label localize label column)

        targets
        (set/intersection (into architectures fixed-architectures) (or architecture-ids #{column-id}))

        fixed (set/intersection targets (or fixed-architectures #{}))
        editable (set/difference targets fixed)
        default (contains? selected platform)

        enabled
        (coll/into-> editable fixed (filter #(get-in state [:graphics id %] default)))

        checked (= enabled targets)
        mixed (and (not checked) (not (coll/empty? enabled)))

        fixed-tooltip
        (localize (localization/message "form.app-manifest.fixed-simulator-backend"))]
    {:fx/type fx.stack-pane/lifecycle
     :style-class (cond-> ["matrix-graphics-cell"] (:group-start column) (conj "matrix-group-start"))
     :pseudo-classes (if (= column-id (:column active-guide)) #{:guided} #{})
     :pick-on-bounds true
     :on-mouse-entered (fn guide-hovered-cell [_event] (track-guide! :hovered cell true))
     :on-mouse-exited (fn unguide-hovered-cell [_event] (track-guide! :hovered cell false))
     :children (cond
                 (coll/empty? targets)
                 [{:fx/type fxui/paragraph
                   :alignment :center
                   :color :hint
                   :text "·"
                   accessible-text (localize (localization/message "form.app-manifest.unavailable-cell"
                                                                   {"setting" name}))}]

                 (coll/empty? editable)
                 [{:fx/type fxui/paragraph
                   :style-class "matrix-fixed-graphics"
                   :alignment :center
                   :tooltip fixed-tooltip
                   accessible-text fixed-tooltip
                   :graphic {:fx/type fx.stack-pane/lifecycle
                             :pref-width 54
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
                                         :stack-pane/margin {:right 3}}]}}]

                 :else
                 [{:fx/type fxui/check-box
                   accessible-text name
                   :max-width :use-pref-size
                   :selected checked
                   :indeterminate mixed
                   :on-focused-changed (fn guide-focused-cell [focused]
                                         (track-guide! :focused cell focused))
                   :on-action (fn toggle-backends [^ActionEvent event]
                                (doto ^CheckBox (.getSource event)
                                  (.setSelected checked)
                                  (.setIndeterminate mixed))
                                (swap-state
                                  (fn [state]
                                    (reduce (fn [s architecture]
                                              (coll/removing-assoc-in s [:graphics id architecture]
                                                                      (when (= checked default) (not checked))))
                                            state
                                            editable))))}])}))

(defn- matrix-layout [label-width widths]
  {:width (reduce + label-width widths)
   :constraints
   (coll/into-> widths [{:fx/type fx.column-constraints/lifecycle :pref-width label-width}]
     (map (fn [width]
            {:fx/type fx.column-constraints/lifecycle :pref-width width :halignment :center})))})

(defn- feature-layout [columns]
  (matrix-layout 252
                 (e/map (fn [{:keys [id generic]}]
                          (cond
                            (= :all id) 64
                            generic 98
                            :else 88))
                        columns)))

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

(defn- prepare-rows [localize rows]
  (let [row-by-id (coll/pair-map-by :id identity rows)]
    (mapv (fn [{:keys [label parent alternative] :as row}]
            (let [text (localize label)
                  parents (parent-rows row-by-id row)
                  lineage (conj parents row)]
              (assoc row
                :display-label text
                :text (if parent
                        (localize (localization/message "form.app-manifest.child-feature"
                                                        {"parent" (localize (:label (row-by-id parent)))
                                                         "feature" text}))
                        text)
                :depth (count parents)
                :supported (reduce set/intersection all-platforms (e/keep :supported lineage))
                :supported-variants (reduce set/intersection all-variants
                                            (e/keep :supported-variants lineage))
                :alternative-row (row-by-id alternative)
                :prerequisites parents)))
          rows)))

(defn- platform-options [localize]
  (mapv
    (fn [[platform architectures]]
      {:label platform
       :columns
       (coll/into-> architectures
         [{:id platform
           :platform platform
           :generic true
           :architecture-ids (coll/into-> architectures #{} (map first))
           :label platform}]
         (map (fn [[id architecture]]
                (let [label (localize architecture)]
                  {:id id
                   :platform platform
                   :label (str platform " " label)
                   :menu-label label}))))})
    platform-groups))

(defn- confirm-reset! [localize reset-preview! section ^ActionEvent event]
  (let [source (.getSource event)
        title (localization/message "form.app-manifest.reset-defaults")

        owner
        (if (instance? MenuItem source)
          (some-> ^MenuItem source .getParentPopup .getOwnerWindow)
          (.. ^Node source getScene getWindow))]
    (when (dialogs/make-confirmation-dialog
            localize
            {:owner owner
             :title title
             :header (localization/message "form.app-manifest.reset-section-confirmation" {"section" section})
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
      (reset-preview!))))

(defn- copyable-settings [rows overrides target]
  (coll/into-> overrides {}
    (keep (fn [[row-id values]]
            (let [{:keys [supported supported-variants]} (rows row-id)]
              (when (supported-column? supported target)
                (when-let [values (coll/not-empty (select-keys values supported-variants))]
                  [row-id values])))))))

(defn- column-menu [localize settings rows all-columns update-settings! column]
  (let [{:keys [id]} column

        copy-items
        (coll/into-> all-columns []
          (remove #(= id (:id %)))
          (keep (fn [{source-id :id :keys [label]}]
                  (when-let [overrides (coll/not-empty (get settings source-id))]
                    (when-let [values (coll/not-empty (copyable-settings rows overrides column))]
                      {:fx/type fx.menu-item/lifecycle
                       :text label
                       :on-action (fn copy-column [_event]
                                    (update-settings! #(assoc % id values)))})))))

        label
        (localize (localization/message "form.app-manifest.column-actions" {"target" (:label column)}))]
    {:fx/type fxui/menu-button
     :style-class "matrix-actions"
     accessible-text label
     :tooltip label
     :items [{:fx/type fx.menu/lifecycle
              :text (localize (localization/message "form.app-manifest.copy-from"))
              :disable (coll/empty? copy-items)
              :items copy-items}
             {:fx/type fx.menu-item/lifecycle
              :text (localize (localization/message "form.app-manifest.reset-defaults"))
              :disable (coll/empty? (get settings id))
              :on-action (fn reset-column [_event]
                           (update-settings! #(dissoc % id)))}]}))

(defn- column-heading [label menu style-class]
  {:fx/type fxui/horizontal
   :style-class style-class
   :alignment :center
   :spacing 2
   :children (cond-> [{:fx/type fxui/paragraph :text label :alignment :center :text-alignment :center}]
               menu (conj menu))})

(defn- visible-platform-groups [options customized expanded]
  (mapv (fn [{:keys [label columns] :as group}]
          (let [architectures (subvec columns 1)
                revealable (< 1 (count architectures))

                open
                (and revealable (get expanded label (coll/any? #(customized (:id %)) architectures)))

                visible (if open architectures [(first columns)])]
            (assoc group
              :open open
              :revealable revealable
              :generic-column (first columns)
              :columns (assoc-in visible [0 :group-start] true))))
        options))

(defn- matrix-headings [localize groups all-column menu-for toggle-platform! active-guide]
  (let [multi-row (coll/any? :open groups)

        initial-headings
        (if all-column
          [(assoc (column-heading (:label all-column) (menu-for all-column) "matrix-platform-heading")
             :grid-pane/column 1
             :grid-pane/row-span (if multi-row 2 1)
             :pseudo-classes (if (= :all (:column active-guide)) #{:guided} #{}))]
          [])]
    (second
      (reduce
        (fn [[column-index headings] {:keys [label columns open revealable] :as group}]
          (let [generic (:generic-column group)
                span (count columns)

                disclosure-label
                (localize (localization/message (if open
                                                  "form.app-manifest.hide-architectures"
                                                  "form.app-manifest.show-architectures")
                                                {"platform" label}))

                heading
                (cond-> (column-heading label (menu-for generic) ["matrix-platform-heading" "matrix-group-start"])
                  true (assoc :grid-pane/column column-index
                              :grid-pane/column-span span
                              :grid-pane/row-span (if (and multi-row (= 1 span)) 2 1)
                              :pseudo-classes (if (= label (:platform active-guide)) #{:guided} #{}))
                  revealable (update :children conj
                                     {:fx/type fxui/button
                                      :style-class "matrix-disclosure"
                                      :text (if open "−" "+")
                                      accessible-text disclosure-label
                                      :tooltip disclosure-label
                                      :focus-traversable true
                                      :on-action (fn toggle-platform [_event] (toggle-platform! label (not open)))}))

                subheadings
                (if-not open
                  []
                  (coll/into-> columns []
                    (map-indexed (fn [i arch]
                                   (-> (column-heading (:menu-label arch) (menu-for arch) "matrix-architecture-heading")
                                       (cond-> (zero? i) (fxui/add-style-classes "matrix-group-start"))
                                       (assoc :grid-pane/column (+ column-index i)
                                              :grid-pane/row 1
                                              :pseudo-classes (if (= (:id arch) (:column active-guide)) #{:guided} #{})))))))]
            [(+ column-index span) (into (conj headings heading) subheadings)]))
        [(if all-column 2 1) initial-headings]
        groups))))

(defn- section-heading [localize title groups reset-section!]
  {:fx/type fxui/horizontal
   :style-class "matrix-row-label"
   :view-order -1
   :grid-pane/row-span (if (coll/any? :open groups) 2 1)
   :padding {:left 8}
   :spacing 4
   :alignment :center-left
   :children [{:fx/type fxui/paragraph :style-class "cljfx-form-group-title" :text title}
              {:fx/type fxui/menu-button
               :style-class "matrix-actions"
               accessible-text (localize (localization/message "form.app-manifest.section-actions" {"section" title}))
               :items [{:fx/type fx.menu-item/lifecycle
                        :text (localize (localization/message "form.app-manifest.reset-defaults"))
                        :on-action (fn/partial confirm-reset! localize reset-section! title)}]}]})

(defn- toggle-row-from-click! [toggle! ^MouseEvent event]
  (when (= MouseButton/PRIMARY (.getButton event))
    (let [row (.getSource event)]
      (loop [^Node node (.. event getPickResult getIntersectedNode)]
        (cond
          (identical? node row) (toggle!)
          (and node (not (instance? ButtonBase node))) (recur (.getParent node)))))))

(defn- feature-matrix-row [localize columns constraints render-cell i {:keys [id text display-label depth] :as row}
                           target-variants variant-label toggle! open {:keys [active-guide track-guide!]}]
  (let [row-key [id (when variant-label (first target-variants))]

        label
        (if variant-label
          {:fx/type fxui/paragraph
           :style-class "matrix-row-label"
           :padding {:left (+ 42 (* depth 20))}
           :alignment :center-left
           :text variant-label}

          {:fx/type fxui/horizontal
           :style-class "matrix-row-label"
           :padding {:left (+ 8 (* depth 20))}
           :children [{:fx/type fxui/button
                       :style-class "matrix-row-toggle"
                       :alignment :center-left
                       :h-box/hgrow :always
                       :max-width Double/MAX_VALUE
                       :max-height Double/MAX_VALUE
                       :focus-traversable true
                       accessible-text text
                       :text (str (if open "▾  " "▸  ") display-label)
                       :on-action (fn toggle-feature [_event] (toggle!))}]})

        setting
        (cond-> row
          variant-label (assoc :text (localize (localization/message "form.app-manifest.feature-variant"
                                                                     {"feature" text "variant" variant-label}))))

        cells
        (e/map (fn [{column-id :id :keys [group-start platform] :as column}]
                 (let [cell {:row row-key :column column-id :platform platform}]
                   {:fx/type fx.stack-pane/lifecycle
                    :style-class (cond-> ["matrix-feature-cell"] group-start (conj "matrix-group-start"))
                    :pseudo-classes (if (= column-id (:column active-guide)) #{:guided} #{})
                    :pick-on-bounds true
                    :on-mouse-entered (fn guide-cell [_event] (track-guide! :hovered cell true))
                    :on-mouse-exited (fn clear-cell [_event] (track-guide! :hovered cell false))
                    :children [(assoc (render-cell setting target-variants column)
                                 :on-focused-changed (fn guide-focus [focused]
                                                       (track-guide! :focused cell focused)))]}))
               columns)]
    (cond-> (matrix-row constraints (inc i) (assoc label :view-order -1) cells (= row-key (:row active-guide)))
      true (assoc :fx/key row-key :pick-on-bounds true)
      variant-label (update :pseudo-classes conj :variant)
      open (update :pseudo-classes conj :open)
      toggle! (assoc :on-mouse-clicked (fn handle-row-click [event] (toggle-row-from-click! toggle! event))))))

(ui/defc ^:private feature-table-view
  {:compose [{:fx/type fx/ext-state :initial-state {}}]}
  [{:keys [state swap-state localize preview-state swap-preview-state settings section
           all-column all-columns groups columns width constraints]}]
  (let [{:keys [id title rows empty-label]} section
        prepared (prepare-rows localize rows)
        row-by-id (coll/pair-map-by :id identity prepared)
        section-name (localize title)
        active-table (:guide-table preview-state)

        active-guide
        (when (= id active-table) (or (:hovered state) (:focused state)))

        guide
        {:active-guide active-guide
         :track-guide! (fn/partial track-cell-guide! id active-table swap-preview-state swap-state)}

        update-settings!
        (fn update-settings! [f] (swap-preview-state update-in [:settings id] f))

        toggle-platform!
        (fn toggle-platform! [platform open]
          (swap-preview-state assoc-in [:expanded-platforms id platform] open))

        reset-section!
        (fn reset-section! []
          (swap-preview-state (fn [s]
                                (cond-> (-> s
                                            (update :settings dissoc id)
                                            (update :expanded-platforms dissoc id))
                                  (= id (first (:expanded-row s))) (dissoc :expanded-row)))))

        render-cell
        (fn render-cell [row target column]
          (feature-cell localize settings update-settings! row target column))

        title-heading
        (section-heading localize section-name groups reset-section!)

        header
        (assoc (matrix-row constraints 0 title-heading [] false)
          :fx/key :header
          :children (into [title-heading]
                          (matrix-headings localize groups all-column
                                           (fn/partial column-menu localize settings row-by-id all-columns update-settings!)
                                           toggle-platform!
                                           active-guide)))

        body
        (if (and empty-label (coll/empty? rows))
          [header {:fx/type fxui/paragraph
                   :fx/key :empty
                   :padding {:top 6 :left 8 :bottom 6}
                   :text (localize empty-label)}]
          (coll/into-> prepared [header]
            (map-indexed
              (fn [i row]
                (let [key [id (:id row)]
                      open (= key (:expanded-row preview-state))

                      toggle!
                      (fn toggle-feature-row []
                        (swap-preview-state (fn [s] (if (= key (:expanded-row s))
                                                      (dissoc s :expanded-row)
                                                      (assoc s :expanded-row key)))))

                      main-row
                      (feature-matrix-row localize columns constraints render-cell i row all-variants nil toggle! open guide)]
                  (if-not open
                    [main-row]
                    (coll/into-> variants [main-row]
                      (map (fn [[variant label]]
                             (feature-matrix-row localize columns constraints render-cell i row
                                                 #{variant} (localize label) nil false guide))))))))
            cat))]
    {:fx/type fxui/vertical
     :style-class ["app-manifest-matrix" "app-manifest-feature-matrix"]
     :max-width width
     :children body}))

(defn- feature-table [localize state swap-state options {:keys [id] :as section}]
  (let [settings (get-in state [:settings id])
        expanded (get-in state [:expanded-platforms id])

        groups
        (visible-platform-groups options #(contains? settings %) expanded)

        all-column
        {:id :all :label (localize (localization/message "form.app-manifest.all-platforms"))}

        columns (coll/into-> groups [all-column] (mapcat :columns))
        all-columns (coll/into-> options [all-column] (mapcat :columns))
        {:keys [width constraints]} (feature-layout columns)]
    {:width width
     :view {:fx/type feature-table-view
            :fx/key id
            :localize localize
            :preview-state state
            :swap-preview-state swap-state
            :settings settings
            :section section
            :all-column all-column
            :all-columns all-columns
            :groups groups
            :columns columns
            :width width
            :constraints constraints}}))

(defn- update-pinned-matrices! [^ScrollPane scroll]
  (when-let [^VBox content (.getContent scroll)]
    (let [bounds (.getLayoutBounds content)
          viewport (.getViewportBounds scroll)

          scroll-x
          (Math/round (* (.getHvalue scroll) (max 0.0 (- (.getWidth bounds) (.getWidth viewport)))))

          scroll-y
          (Math/round (* (.getVvalue scroll) (max 0.0 (- (.getHeight bounds) (.getHeight viewport)))))]
      (doseq [^VBox table (.getChildren content)
              :when (.contains (.getStyleClass table) "app-manifest-matrix")]
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
  [{:keys [state swap-state localize preview-state swap-preview-state columns groups layout]}]
  (let [active-table (:guide-table preview-state)
        title (localize (:title graphics-matrix))

        guide
        (when (= :graphics active-table) (or (:hovered state) (:focused state)))

        track-guide!
        (fn/partial track-cell-guide! :graphics active-table swap-preview-state swap-state)

        cell
        (fn/partial graphics-cell localize preview-state swap-preview-state guide track-guide!)

        reset-section!
        (fn reset-graphics! []
          (swap-preview-state (fn [state]
                                (-> state
                                    (dissoc :graphics)
                                    (update :expanded-platforms dissoc :graphics)))))

        toggle-platform!
        (fn toggle-platform! [platform open]
          (swap-preview-state assoc-in [:expanded-platforms :graphics platform] open))

        {:keys [width constraints]} layout
        title-heading (section-heading localize title groups reset-section!)

        header
        (assoc (matrix-row constraints 0 title-heading [] false)
          :children (into [title-heading]
                          (matrix-headings localize groups nil (constantly nil) toggle-platform! guide)))]
    {:fx/type fxui/vertical
     :style-class ["app-manifest-matrix" "app-manifest-graphics"]
     :max-width width
     :children (coll/into-> (:rows graphics-matrix) [header]
                 (map-indexed (fn [i {:keys [id label] :as row}]
                                (matrix-row
                                  constraints
                                  (inc i)
                                  {:fx/type fxui/paragraph
                                   :style-class "matrix-row-label"
                                   :view-order -1
                                   :padding {:left 8}
                                   :alignment :center-left
                                   :text label}
                                  (e/map (fn/partial cell row) columns)
                                  (= id (:row guide))))))}))

(ui/defc ^:private features
  {:compose [{:fx/type fx/ext-state :initial-state {}}
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

        options (platform-options localization-state)

        graphics-groups
        (visible-platform-groups options (constantly false) (get-in state [:expanded-platforms :graphics]))

        graphics-columns (coll/into-> graphics-groups [] (mapcat :columns))

        graphics-layout
        (matrix-layout 190 (e/map #(if (:generic %) 98 56) graphics-columns))

        feature-options
        (mapv (fn [{:keys [columns] :as group}]
                (cond-> group
                  (= 2 (count columns)) (assoc :columns (subvec columns 0 1))))
              options)

        graphics
        {:fx/type graphics-table
         :localize localization-state
         :preview-state state
         :swap-preview-state swap-state
         :columns graphics-columns
         :groups graphics-groups
         :layout graphics-layout}

        tables
        (mapv (fn/partial feature-table localization-state state swap-state feature-options)
              (conj feature-matrices
                    {:id :extensions
                     :title (localization/message "form.app-manifest.native-extensions")
                     :empty-label (localization/message "form.app-manifest.native-extensions-empty")
                     :rows native-extension-rows}))

        content-width
        (+ 48 (transduce (map :width) max (:width graphics-layout) tables))]
    {:fx/type fxui/scroll
     :id "scroll-pane"
     pinned-matrices true
     :content {:fx/type fxui/vertical
               :padding :medium
               :spacing 20
               :min-width content-width
               :max-width (max 920 content-width)
               :children (coll/into-> tables
                           [(assoc header :fx/key :intro)
                            (assoc graphics :fx/key :graphics)]
                           (map :view))}}))

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
