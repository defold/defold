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
            [cljfx.lifecycle :as fx.lifecycle]
            [cljfx.mutator :as fx.mutator]
            [dynamo.graph :as g]
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
  (:import [javafx.event ActionEvent]
           [javafx.scene Node]
           [javafx.scene.control CheckBox]))

(set! *warn-on-reflection* true)

(def ^:private accessible-text
  (fx/make-prop (fx.mutator/setter (fn set-accessible-text [^Node node text]
                                     (.setAccessibleText node text)))
                fx.lifecycle/scalar))

(def ^:private platforms ["macOS" "Windows" "Linux" "iOS" "Android" "HTML5"])

(def ^:private all-platforms (set platforms))

(def ^:private desktop-platforms #{"macOS" "Windows" "Linux"})

(def ^:private variants
  [[:release (localization/message "dialog.bundle.option.variant.release")]
   [:debug (localization/message "dialog.bundle.option.variant.debug")]
   [:headless (localization/message "dialog.bundle.option.variant.headless")]])

(def ^:private graphics-matrix
  {:title (localization/message "form.app-manifest.graphics-backends")
   :rows [{:id :opengl
           :label "OpenGL / OpenGL ES"
           :supported (disj all-platforms "HTML5")
           :selected #{"Windows" "Linux" "iOS" "Android"}}
          {:id :vulkan :label "Vulkan" :supported (disj all-platforms "HTML5") :selected #{"macOS" "Android"}}
          {:id :metal :label "Metal" :supported #{"macOS" "iOS"}}
          {:id :dx12 :label "DirectX 12" :supported #{"Windows"}}
          {:id :webgl :label "WebGL" :supported #{"HTML5"} :selected #{"HTML5"}}
          {:id :webgpu :label "WebGPU" :supported #{"HTML5"}}]})

(def ^:private feature-matrices
  [{:title (localization/message "form.group.components")
    :rows [{:id :sound :label (localization/message "resource.type.sound") :selected all-platforms}
           {:id :wav :label "WAV" :parent :sound :selected all-platforms}
           {:id :ogg :label "OGG" :parent :sound :selected all-platforms}
           {:id :opus :label "OPUS" :parent :sound :selected #{"Android"} :selected-variants #{:debug}}
           {:id :gui :label (localization/message "resource.type.gui") :selected all-platforms}
           {:id :tilemap :label (localization/message "form.label.project.tilemap") :selected all-platforms}
           {:id :label :label (localization/message "resource.type.label") :selected all-platforms}
           {:id :particlefx :label (localization/message "resource.type.particlefx") :selected all-platforms}
           {:id :rig :label (localization/message "form.app-manifest.rig") :selected all-platforms}
           {:id :model :label (localization/message "resource.type.model") :parent :rig :selected all-platforms}]}

   {:title (localization/message "form.group.runtime")
    :rows [{:id :physics :label (localization/message "form.label.project.physics") :selected all-platforms}
           {:id :box2d-v2 :label "Box2D v2" :parent :physics :alternative :box2d-v3}
           {:id :script-box2d-v2 :label (localization/message "form.app-manifest.lua-api") :parent :box2d-v2 :selected all-platforms}
           {:id :box2d-v3 :label "Box2D v3" :parent :physics :alternative :box2d-v2 :selected all-platforms}
           {:id :script-box2d-v3 :label (localization/message "form.app-manifest.lua-api") :parent :box2d-v3 :selected all-platforms}
           {:id :bullet :label "Bullet 3D" :parent :physics :selected all-platforms}
           {:id :script-bullet :label (localization/message "form.app-manifest.lua-api") :parent :bullet :selected all-platforms}
           {:id :input :label (localization/message "form.label.project.input") :selected all-platforms}
           {:id :liveupdate :label (localization/message "form.label.project.liveupdate") :selected all-platforms}
           {:id :datastore :label (localization/message "form.app-manifest.datastore") :selected all-platforms}
           {:id :basis-transcoder
            :label (localization/message "form.app-manifest.basis-transcoder")
            :selected all-platforms}
           {:id :android-support :label (localization/message "form.app-manifest.android-support") :supported #{"Android"}}
           {:id :image :label (localization/message "form.app-manifest.image-module") :selected all-platforms}
           {:id :script-types :label (localization/message "form.app-manifest.script-types") :selected all-platforms}
           {:id :font-layout :label (localization/message "form.app-manifest.full-text-layout")}
           {:id :rich-text :label (localization/message "form.app-manifest.rich-text") :selected all-platforms}
           {:id :profiler
            :label (localization/message "property.appmanifest.profiler")
            :selected all-platforms
            :selected-variants #{:debug}}
           {:id :profiler-basic
            :label (localization/message "form.app-manifest.profiler-basic")
            :parent :profiler
            :selected all-platforms}
           {:id :profiler-remotery
            :label (localization/message "form.app-manifest.profiler-remotery")
            :parent :profiler
            :supported (disj all-platforms "HTML5")
            :selected (disj all-platforms "HTML5")}
           {:id :profiler-js
            :label (localization/message "form.app-manifest.profiler-js")
            :parent :profiler
            :supported #{"HTML5"}
            :selected #{"HTML5"}}
           {:id :record
            :label (localization/message "form.app-manifest.recording")
            :supported desktop-platforms
            :supported-variants #{:release :debug}
            :selected desktop-platforms
            :selected-variants #{:debug}}
           {:id :record-script
            :label (localization/message "form.app-manifest.lua-api")
            :parent :record
            :selected desktop-platforms}
           {:id :record-service
            :label (localization/message "form.app-manifest.recording-service")
            :parent :record
            :supported-variants #{:debug}
            :selected desktop-platforms}]}])

(defn- selected-feature-ids [rows platform variant]
  (coll/into-> rows #{}
    (filter (fn [{:keys [selected selected-variants supported-variants]}]
              (and (contains? selected platform)
                   (or (nil? supported-variants) (contains? supported-variants variant))
                   (or (nil? selected-variants) (contains? selected-variants variant)))))
    (map :id)))

(def ^:private sample-selections
  {:features (coll/pair-map-by
               identity
               (fn [platform]
                 (coll/pair-map-by
                   first
                   (fn [[variant]]
                     (selected-feature-ids (e/mapcat :rows feature-matrices) platform variant))
                   variants))
               platforms)})

(defn- set-feature [selection id alternative enabled]
  (if-not enabled
    (disj selection id)
    (cond-> (conj selection id)
      alternative (disj alternative))))

(defn- set-platform-variants [state id alternative platform target-variants enabled]
  (reduce (fn [state variant]
            (update-in state [:features platform variant] set-feature id alternative enabled))
          state
          target-variants))

(defn- feature-variant-selection
  [state {:keys [id supported supported-variants prerequisites]} platform]
  (let [selections (get-in state [:features platform])

        available
        (if (contains? supported platform)
          (coll/into-> variants #{}
            (map first)
            (filter (fn [variant]
                      (and (or (nil? supported-variants) (contains? supported-variants variant))
                           (coll/every? (selections variant) prerequisites)))))
          #{})]
    {:available available
     :selected (coll/into-> available #{} (filter #(contains? (selections %) id)))}))

(defn- matrix-toggle [props selected-count total-count available-count set-enabled!]
  (let [checked (and (pos? total-count) (= total-count selected-count))
        mixed (and (not checked) (pos? selected-count))]
    (assoc props
      :fx/type fxui/check-box
      :max-width :use-pref-size
      :disable (zero? available-count)
      :selected checked
      :indeterminate mixed
      :on-action (fn toggle-cell [^ActionEvent event]
                   ;; Restore the checkbox state in case the next selection
                   ;; also renders as mixed.
                   (doto ^CheckBox (.getSource event)
                     (.setSelected checked)
                     (.setIndeterminate mixed))
                   (set-enabled! (not= selected-count available-count))))))

(defn- all-platforms-props [localize text]
  (let [label
        (localize (localization/message "form.app-manifest.all-platforms-for-feature" {"feature" text}))]
    {accessible-text label
     :tooltip label}))

(defn- variant-menu-item
  [localize platform-count available selected set-variants [variant label]]
  (let [text (localize label)
        available-count (get available variant 0)
        selected-count (get selected variant 0)
        checked (= platform-count selected-count)]
    {:fx/type fx.custom-menu-item/lifecycle
     :text text
     :disable (zero? available-count)
     :hide-on-click false
     :on-action (fn toggle-variant [_event]
                  (set-variants #{variant} (not= available-count selected-count)))
     :content {:fx/type fxui/horizontal
               :pick-on-bounds true
               :children [{:fx/type fxui/check-box
                           :text text
                           :mouse-transparent true
                           :focus-traversable false
                           :selected checked
                           :indeterminate (and (not checked) (pos? selected-count))}]}}))

(defn- variant-cell [localize props menu-label platform-count selections set-variants]
  (let [available (frequencies (e/mapcat :available selections))
        selected (frequencies (e/mapcat :selected selections))
        selected-count (transduce (map val) + 0 selected)
        available-count (transduce (map val) + 0 available)

        mark-classes
        (coll/into-> selected #{}
          (map (fn [[variant count]]
                 (keyword (str (name variant) (if (= platform-count count) "-full" "-partial"))))))]
    {:fx/type fxui/horizontal
     :alignment :center
     :spacing 3
     :children [(matrix-toggle
                  (assoc props :style-class "matrix-variant-toggle" :pseudo-classes mark-classes)
                  selected-count
                  (* platform-count (count variants))
                  available-count
                  (fn set-enabled! [enabled]
                    (set-variants (coll/into-> available #{} (map key)) enabled)))
                {:fx/type fxui/menu-button
                 :style-class "matrix-variants"
                 accessible-text menu-label
                 :tooltip menu-label
                 :disable (zero? available-count)
                 :items (mapv
                          (fn/partial variant-menu-item localize platform-count available selected set-variants)
                          variants)}]}))

(defn- feature-cell
  [localize state swap-state {:keys [id text alternative] :as row} platform]
  (variant-cell
    localize
    {accessible-text (str text " (" platform ")")}
    (localize (localization/message "form.app-manifest.variant-checklist"
                                    {"feature" text "platform" platform}))
    1
    [(feature-variant-selection state row platform)]
    (fn set-variants [target-variants enabled]
      (swap-state set-platform-variants id alternative platform target-variants enabled))))

(defn- all-feature-cell
  [localize state swap-state {:keys [id text alternative supported] :as row}]
  (let [selections
        (coll/pair-map-by identity (fn/partial feature-variant-selection state row) supported)]
    (variant-cell
      localize
      (all-platforms-props localize text)
      (localize (localization/message "form.app-manifest.all-platforms-variant-checklist"
                                      {"feature" text}))
      (count supported)
      (e/map val selections)
      (fn set-variants [target-variants enabled]
        (swap-state
          (fn update-selections [state]
            (reduce-kv (fn [state platform {:keys [available]}]
                         (set-platform-variants state id alternative platform (e/filter target-variants available) enabled))
                       state
                       selections)))))))

(defn- platform-selected? [state section {:keys [id selected]} platform]
  (get-in state [section id platform] (contains? selected platform)))

(defn- platform-cell [section state swap-state {:keys [id text supported] :as row} platform]
  (let [checked (platform-selected? state section row platform)]
    {:fx/type fxui/check-box
     accessible-text (str text " (" platform ")")
     :max-width :use-pref-size
     :disable (not (contains? supported platform))
     :selected checked
     :on-action (fn toggle-platform [_event]
                  (swap-state assoc-in [section id platform] (not checked)))}))

(defn- all-extension-cell
  [localize state swap-state {:keys [id text supported] :as row}]
  (let [platform-count (count supported)]
    (matrix-toggle
      (all-platforms-props localize text)
      (count (coll/filterv-> supported (fn/partial platform-selected? state :extensions row)))
      platform-count
      platform-count
      (fn set-enabled! [enabled]
        (swap-state update-in [:extensions id] merge
                    (coll/pair-map-by identity (constantly enabled) supported))))))

(def ^:private column-constraints
  (coll/into-> platforms [{:fx/type fx.column-constraints/lifecycle :percent-width 26}
                          {:fx/type fx.column-constraints/lifecycle :percent-width 8 :halignment :center}]
    (map (constantly {:fx/type fx.column-constraints/lifecycle :percent-width 11 :halignment :center}))))

(defn- matrix-row [index label all-cell cells]
  {:fx/type fxui/grid
   :style-class (if (zero? index) "matrix-header" "matrix-row")
   :pseudo-classes (if (odd? index) #{:odd} #{})
   :column-constraints column-constraints
   :children (coll/into-> cells
               (if all-cell
                 [label (assoc all-cell :grid-pane/column 1)]
                 [(assoc label :grid-pane/column-span 2)])
               (map-indexed (fn [i cell] (assoc cell :grid-pane/column (+ 2 i)))))})

(defn- parent-ids [row-by-id row]
  (loop [ids []
         parent (:parent row)]
    (if parent
      (recur (conj ids parent) (:parent (row-by-id parent)))
      ids)))

(defn- feature-row [localize render-cell render-all-cell row-by-id index {:keys [label parent] :as row}]
  (let [text (localize label)
        prerequisites (parent-ids row-by-id row)

        control-name
        (if parent
          (localize (localization/message "form.app-manifest.child-feature"
                                          {"parent" (localize (:label (row-by-id parent)))
                                           "feature" text}))
          text)

        cell
        (assoc row
          :text control-name
          :supported (get row :supported all-platforms)
          :prerequisites prerequisites)]
    (matrix-row
      (inc index)
      {:fx/type fxui/paragraph
       :grid-pane/margin {:left (+ 8 (* 20 (count prerequisites)))}
       :alignment :center-left
       :text text}
      (when render-all-cell (render-all-cell cell))
      (e/map (fn/partial render-cell cell) platforms))))

(defn- matrix [localize render-cell render-all-cell {:keys [title rows empty-label]}]
  (let [header
        (matrix-row
          0
          {:fx/type fxui/paragraph
           :grid-pane/margin {:left 8}
           :alignment :center-left
           :style-class "cljfx-form-group-title"
           :text (localize title)}
          (when render-all-cell
            {:fx/type fxui/label
             :alignment :center
             :text (localize (localization/message "form.app-manifest.all-platforms"))})
          (e/map (fn [platform] {:fx/type fxui/label :text platform :alignment :center}) platforms))

        row-by-id (coll/pair-map-by :id identity rows)]
    {:fx/type fxui/vertical
     :style-class "app-manifest-matrix"
     :children (if (and empty-label (coll/empty? rows))
                 [header {:fx/type fxui/paragraph
                          :padding {:top 6 :left 8 :bottom 6}
                          :text (localize empty-label)}]
                 (coll/into-> rows [header]
                   (map-indexed (fn/partial feature-row localize render-cell render-all-cell row-by-id))))}))

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

        graphics
        (matrix localization-state (fn/partial platform-cell :graphics state swap-state) nil graphics-matrix)

        sections
        (coll/into-> feature-matrices [header graphics]
          (map (fn/partial matrix
                           localization-state
                           (fn/partial feature-cell localization-state state swap-state)
                           (fn/partial all-feature-cell localization-state state swap-state))))

        extensions
        (matrix
          localization-state
          (fn/partial platform-cell :extensions state swap-state)
          (fn/partial all-extension-cell localization-state state swap-state)
          {:title (localization/message "form.app-manifest.native-extensions")
           :empty-label (localization/message "form.app-manifest.native-extensions-empty")
           :rows native-extension-rows})]
    {:fx/type fxui/scroll
     :id "scroll-pane"
     :content {:fx/type fxui/vertical
               :padding :medium
               :spacing 20
               :min-width 560
               :max-width 920
               :children (conj sections extensions)}}))

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
                   :selected all-platforms})))))))

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
