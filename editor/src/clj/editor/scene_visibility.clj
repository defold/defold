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

(ns editor.scene-visibility
  (:require [clojure.set :as set]
            [dynamo.graph :as g]
            [editor.handler :as handler]
            [editor.keymap :as keymap]
            [editor.localization :as localization]
            [editor.os :as os]
            [editor.system :as system]
            [editor.types :as types]
            [editor.ui :as ui]
            [editor.ui.settings-popup :as settings-popup]
            [internal.util :as iutil]
            [schema.core :as s]
            [util.coll :as coll])
  (:import [javafx.geometry Point2D Pos]
           [javafx.css PseudoClass]
           [javafx.scene Parent]
           [javafx.scene.control CheckBox Label PopupControl Separator Tab ToggleButton]
           [javafx.scene.layout HBox Priority Region StackPane VBox]
           [javafx.stage PopupWindow$AnchorLocation]))

(set! *warn-on-reflection* true)

(def ^:private renderable-tag-toggles-info
  (cond-> [{:type :toggle :label "scene-popup.scene-visibility.visibility-filters" :tag :visibility-filters-enabled? :command :scene.visibility.toggle-filters :always-enabled true}
           {:type :space}
           #_{:label "scene-popup.scene-visibility.gui-elements" :tag :gui} ; This tag exists, but we decided to hide it and put in granular control instead. Add back if we make the toggles hierarchical?
           {:type :toggle :label "scene-popup.scene-visibility.collision-shapes" :tag :collision-shape :style-class "compact-toggle"}
           {:type :toggle :label "scene-popup.scene-visibility.camera" :tag :camera :style-class "compact-toggle"}
           {:type :toggle :label "scene-popup.scene-visibility.gui-bounds" :tag :gui-bounds :style-class "compact-toggle"}
           {:type :toggle :label "scene-popup.scene-visibility.gui-shapes" :tag :gui-shape :style-class "compact-toggle"}
           {:type :toggle :label "scene-popup.scene-visibility.gui-particle-effects" :tag :gui-particlefx :style-class "compact-toggle"}
           {:type :toggle :label "scene-popup.scene-visibility.gui-spine-scenes" :tag :gui-spine :style-class "compact-toggle"}
           {:type :toggle :label "scene-popup.scene-visibility.gui-text" :tag :gui-text :style-class "compact-toggle"}
           {:type :toggle :label "scene-popup.scene-visibility.models" :tag :model :style-class "compact-toggle"}
           {:type :toggle :label "scene-popup.scene-visibility.particle-effects" :tag :particlefx :style-class "compact-toggle"}
           {:type :toggle :label "scene-popup.scene-visibility.skeletons" :tag :skeleton :style-class "compact-toggle"}
           {:type :toggle :label "scene-popup.scene-visibility.spine-scenes" :tag :spine :style-class "compact-toggle"}
           {:type :toggle :label "scene-popup.scene-visibility.sprites" :tag :sprite :style-class "compact-toggle"}
           {:type :toggle :label "scene-popup.scene-visibility.text" :tag :text :style-class "compact-toggle"}
           {:type :toggle :label "scene-popup.scene-visibility.tile-maps" :tag :tilemap :style-class "compact-toggle"}
           {:type :separator}
           {:type :toggle :label "scene-popup.scene-visibility.component-guides" :tag :outline :command :scene.visibility.toggle-component-guides :always-enabled true}]

          (system/defold-dev?)
          (into [{:type :separator}
                 {:type :toggle :label "scene-popup.scene-visibility.scene-visibility-bounds" :tag :dev-visibility-bounds :appear-filtered false}])))

(def ^:private toggleable-filters
  (coll/into-> renderable-tag-toggles-info #{}
    (filter #(and (not (:always-enabled %)) (= (:type %) :toggle)))
    (map :tag)))

(def ^:private appear-filtered-renderable-tags
  (into #{}
        (keep (fn [{:keys [appear-filtered tag]
                    :or {appear-filtered true}}]
                (when appear-filtered
                  tag)))
        renderable-tag-toggles-info))

;; -----------------------------------------------------------------------------
;; SceneVisibilityNode
;; -----------------------------------------------------------------------------
;;
;; A SceneHideHistoryNode manages visibility for a particular scene-resource-id.
;; Objects are identified with outline name paths, which are vectors of string
;; ids from the outline. We use the term "name" here to avoid confusing these
;; with node ids. These are not necessarily names or even strings, but currently
;; the schema enforces strings.
;;
;; The individual tokens are named "node-outline-key" elsewhere, but we use the
;; term "outline name path" in this file to distinguish these from
;; node-outline-key-paths, which include the resource node id at the beginning.

(defn outline-name-path? [value]
  (and (vector? value)
       (every? string? value)))

(def ^:private TOutlineNamePaths #{(s/pred outline-name-path?)})
(def ^:private THideHistory [(s/both TOutlineNamePaths (s/pred seq))])
(g/deftype HideHistory THideHistory)
(g/deftype OutlineNamePaths TOutlineNamePaths)
(g/deftype OutlineNamePathsByBool {s/Bool TOutlineNamePaths})
(g/deftype OutlineNamePathsByNodeID {s/Int (s/both TOutlineNamePaths (s/pred seq))})
(g/deftype SceneHideHistoryData [(s/one s/Int "scene-resource-node") (s/one THideHistory "hide-history")])

(defn- scene-outline-name-paths
  ([scene]
   (scene-outline-name-paths [] scene))
  ([outline-name-path {:keys [node-id children] :as _scene}]
   (mapcat (fn [{child-node-id :node-id child-node-outline-key :node-outline-key :as child-scene}]
             (when (some? child-node-outline-key)
               (if (= node-id child-node-id)
                 (scene-outline-name-paths outline-name-path child-scene)
                 (let [child-outline-name-path (conj outline-name-path child-node-outline-key)]
                   (cons child-outline-name-path
                         (scene-outline-name-paths child-outline-name-path child-scene))))))
           children)))

(def ^:private outline-selection-entry->outline-name-path (comp not-empty vec next :node-outline-key-path))

(g/defnode SceneVisibilityNode
  (property app-view g/NodeID)
  (property visibility-filters-enabled? g/Bool (default true))
  (property filtered-renderable-tags types/RenderableTags (default #{:dev-visibility-bounds}))
  (property ui-check-boxes g/Any (default {}))

  (input active-resource-node+type g/Any)
  (input active-scene g/Any :substitute nil)
  (input outline-selection g/Any :substitute nil)
  (input scene-hide-history-datas SceneHideHistoryData :array)

  (output active-scene-resource-node g/NodeID (g/fnk [active-resource-node+type]
                                                (when (some? active-resource-node+type)
                                                  (let [[node type] active-resource-node+type]
                                                    (when (g/has-output? type :scene)
                                                      node)))))

  (output hidden-outline-name-paths-by-scene-resource-node OutlineNamePathsByNodeID :cached (g/fnk [scene-hide-history-datas]
                                                                                              (into {}
                                                                                                    (keep (fn [[scene-resource-node hide-history]]
                                                                                                            (when-some [hidden-outline-name-paths (not-empty (apply set/union hide-history))]
                                                                                                              [scene-resource-node hidden-outline-name-paths])))
                                                                                                    scene-hide-history-datas)))

  (output hidden-renderable-tags types/RenderableTags :cached (g/fnk [filtered-renderable-tags visibility-filters-enabled?]
                                                                (if visibility-filters-enabled?
                                                                  filtered-renderable-tags
                                                                  (set/intersection filtered-renderable-tags #{:grid :outline}))))

  (output hidden-node-outline-key-paths types/NodeOutlineKeyPaths :cached (g/fnk [hidden-outline-name-paths-by-scene-resource-node]
                                                                            (into #{}
                                                                                  (mapcat (fn [[scene-resource-node hidden-outline-name-paths]]
                                                                                            (map (partial into [scene-resource-node])
                                                                                                 hidden-outline-name-paths)))
                                                                                  hidden-outline-name-paths-by-scene-resource-node)))

  (output hidden-outline-name-paths OutlineNamePaths (g/fnk [active-scene-resource-node hidden-outline-name-paths-by-scene-resource-node]
                                                       (hidden-outline-name-paths-by-scene-resource-node active-scene-resource-node)))

  (output outline-name-paths OutlineNamePaths :cached (g/fnk [active-scene] (set (scene-outline-name-paths active-scene))))

  (output selected-outline-name-paths OutlineNamePaths :cached (g/fnk [outline-selection]
                                                                 (set (into [] (keep outline-selection-entry->outline-name-path) outline-selection))))

  (output unselected-outline-name-paths OutlineNamePaths :cached (g/fnk [selected-outline-name-paths outline-name-paths]
                                                                   (set/difference outline-name-paths selected-outline-name-paths)))
  
  (output unselected-hideable-outline-name-paths OutlineNamePaths :cached (g/fnk [hidden-outline-name-paths unselected-outline-name-paths]
                                                                            (not-empty (set/difference unselected-outline-name-paths hidden-outline-name-paths))))

  (output selected-hideable-outline-name-paths OutlineNamePaths :cached (g/fnk [hidden-outline-name-paths selected-outline-name-paths]
                                                                          (not-empty (set/difference selected-outline-name-paths hidden-outline-name-paths))))

  (output selected-showable-outline-name-paths OutlineNamePaths :cached (g/fnk [hidden-outline-name-paths selected-outline-name-paths]
                                                                          (not-empty (set/intersection selected-outline-name-paths hidden-outline-name-paths))))

  (output last-hidden-outline-name-paths OutlineNamePaths :cached (g/fnk [active-scene-resource-node scene-hide-history-datas]
                                                                    (peek (some (fn [[scene-resource-node hide-history]]
                                                                                  (when (= active-scene-resource-node scene-resource-node)
                                                                                    hide-history))
                                                                                scene-hide-history-datas)))))

(defn make-scene-visibility-node! [view-graph app-view]
  (g/make-node! view-graph SceneVisibilityNode :app-view app-view))

;; -----------------------------------------------------------------------------
;; Per-Object Visibility
;; -----------------------------------------------------------------------------

(g/defnode SceneHideHistoryNode
  (property hide-history HideHistory)
  (input scene-resource-node g/NodeID)
  (output scene-hide-history-data SceneHideHistoryData (g/fnk [hide-history scene-resource-node]
                                                         [scene-resource-node hide-history])))

(defn- find-scene-hide-history-node [scene-visibility scene-resource-node]
  (some (fn [[scene-hide-history-node]]
          (when (some-> (g/node-feeding-into scene-hide-history-node :scene-resource-node) (= scene-resource-node))
            scene-hide-history-node))
        (g/sources-of scene-visibility :scene-hide-history-datas)))

(defn- show-outline-name-paths! [scene-visibility outline-name-paths]
  (assert (set? (not-empty outline-name-paths)))
  (assert (every? outline-name-path? outline-name-paths))
  (let [scene-resource-node (g/node-value scene-visibility :active-scene-resource-node)
        scene-hide-history-node (find-scene-hide-history-node scene-visibility scene-resource-node)]

    ;; Remove the now-visible nodes from the hide history. This ensures the Show
    ;; Last Hidden Objects command works as expected if the user manually shows
    ;; nodes she has previously hidden.
    (g/update-property! scene-hide-history-node :hide-history
                        (fn [hide-history]
                          (into []
                                (keep (fn [hidden-outline-name-paths]
                                        (not-empty (set/difference hidden-outline-name-paths outline-name-paths))))
                                hide-history)))

    ;; Remove the SceneHideHistoryNode if its history is now empty.
    (when (empty? (g/node-value scene-hide-history-node :hide-history))
      (g/delete-node! scene-hide-history-node))))

(defn- hide-outline-name-paths! [scene-visibility outline-name-paths]
  (assert (set? (not-empty outline-name-paths)))
  (assert (every? outline-name-path? outline-name-paths))
  (let [scene-resource-node (g/node-value scene-visibility :active-scene-resource-node)
        scene-hide-history-node (find-scene-hide-history-node scene-visibility scene-resource-node)]
    (if (some? scene-hide-history-node)
      (g/update-property! scene-hide-history-node :hide-history conj outline-name-paths)
      (g/transact
        (g/make-nodes (g/node-id->graph-id scene-visibility)
                      [scene-hide-history-node [SceneHideHistoryNode :hide-history [outline-name-paths]]]
                      (g/connect scene-resource-node :_node-id scene-hide-history-node :scene-resource-node)
                      (g/connect scene-hide-history-node :scene-hide-history-data scene-visibility :scene-hide-history-datas))))))

(handler/defhandler :scene.visibility.hide-unselected :workbench
  (active? [scene-visibility evaluation-context]
    (g/node-value scene-visibility :active-scene-resource-node evaluation-context))
  (enabled? [scene-visibility evaluation-context]
    (g/node-value scene-visibility :unselected-hideable-outline-name-paths evaluation-context))
  (run [scene-visibility] (hide-outline-name-paths! scene-visibility (g/node-value scene-visibility :unselected-hideable-outline-name-paths))))

(handler/defhandler :scene.visibility.toggle-selection :workbench
  (active? [scene-visibility evaluation-context]
    (g/node-value scene-visibility :active-scene-resource-node evaluation-context))
  (enabled? [scene-visibility evaluation-context]
    (or (g/node-value scene-visibility :selected-hideable-outline-name-paths evaluation-context)
        (g/node-value scene-visibility :selected-showable-outline-name-paths evaluation-context)))
  (run [scene-visibility]
    (g/with-auto-evaluation-context evaluation-context
      (let [should-hide (g/node-value scene-visibility :selected-hideable-outline-name-paths evaluation-context)]
        (if should-hide
          (hide-outline-name-paths! scene-visibility (g/node-value scene-visibility :selected-hideable-outline-name-paths))
          (show-outline-name-paths! scene-visibility (g/node-value scene-visibility :selected-showable-outline-name-paths)))))))

(handler/defhandler :private/hide-toggle :workbench
  (active? [scene-visibility evaluation-context user-data]
    (g/node-value scene-visibility :active-scene-resource-node evaluation-context))
  (run [scene-visibility user-data]
    (let [{:keys [node-outline-key-path]} user-data
          name-paths-to-toggle #{(subvec node-outline-key-path 1)}]
      (if (contains? (g/node-value scene-visibility :hidden-node-outline-key-paths) node-outline-key-path)
        (show-outline-name-paths! scene-visibility name-paths-to-toggle)
        (hide-outline-name-paths! scene-visibility name-paths-to-toggle)))))

(handler/defhandler :scene.visibility.show-last-hidden :workbench
  (active? [scene-visibility evaluation-context]
    (g/node-value scene-visibility :active-scene-resource-node evaluation-context))
  (enabled? [scene-visibility evaluation-context]
    (g/node-value scene-visibility :last-hidden-outline-name-paths evaluation-context))
  (run [scene-visibility] (show-outline-name-paths! scene-visibility (g/node-value scene-visibility :last-hidden-outline-name-paths))))

(handler/defhandler :scene.visibility.show-all :workbench
  (active? [scene-visibility evaluation-context]
    (g/node-value scene-visibility :active-scene-resource-node evaluation-context))
  (enabled? [scene-visibility evaluation-context]
    (g/node-value scene-visibility :hidden-outline-name-paths evaluation-context))
  (run [scene-visibility] (show-outline-name-paths! scene-visibility (g/node-value scene-visibility :hidden-outline-name-paths))))

;; -----------------------------------------------------------------------------
;; Visibility Filters
;; -----------------------------------------------------------------------------

(defn toggle-button [app-view]
  (some-> ^Tab (g/node-value app-view :active-tab)
          .getContent
          (.lookup "#visibility-settings-graphic")
          .getParent))

(defn sync-filter-button-style! [app-view scene-visibility evaluation-context]
  (when-let [btn ^ToggleButton (toggle-button app-view)]
    (if (and (g/node-value scene-visibility :visibility-filters-enabled? evaluation-context)
             (some appear-filtered-renderable-tags
                   (g/node-value scene-visibility :filtered-renderable-tags evaluation-context)))
      (.pseudoClassStateChanged btn (PseudoClass/getPseudoClass "filters-active") true)
      (.pseudoClassStateChanged btn (PseudoClass/getPseudoClass "filters-active") false))))

(defn- sync-filter-checkboxes!
  ([scene-visibility]
   (g/with-auto-evaluation-context evaluation-context
     (sync-filter-checkboxes! scene-visibility evaluation-context)))
  ([scene-visibility evaluation-context]
   (sync-filter-button-style! (g/node-value scene-visibility :app-view) scene-visibility evaluation-context)
   (when-let [check-boxes (:filter-check-boxes (g/node-value scene-visibility :ui-check-boxes evaluation-context))]
     (when (seq check-boxes)
       (let [enabled? (g/node-value scene-visibility :visibility-filters-enabled? evaluation-context)]
         (doseq [cb check-boxes]
           (ui/enable! cb enabled?)))))))

(defrecord SceneVisibilityStore [scene-visibility]
  settings-popup/SettingsStore
  (get-value [_ key]
    (if (= :visibility-filters-enabled? key)
      (g/node-value scene-visibility :visibility-filters-enabled?)
      (not (contains? (g/node-value scene-visibility :filtered-renderable-tags) key))))
  (get-default-value [_ _] true)
  (set-value! [_ key v]
    (if (= :visibility-filters-enabled? key)
      (g/set-property! scene-visibility :visibility-filters-enabled? v)
      (g/update-property! scene-visibility :filtered-renderable-tags (if v disj conj) key))
    (sync-filter-checkboxes! scene-visibility)))

(defn show-settings! [app-view keymap localization ^Parent owner scene-visibility]
  (let [setting-descriptors (mapv #(-> %
                                       (assoc :key (:tag %))
                                       (dissoc :tag :always-enabled :appear-filtered))
                                  renderable-tag-toggles-info)
        scene-vis-store (->SceneVisibilityStore scene-visibility)
        ;; HACK: There's a visual bug where if you're hovering over the SplitPane next to Outline, if you move the
        ;; cursor into the popup, JavaFX doesn't receive a mouse-move inside the scene view, so once you
        ;; enter the popup, the H_RESIZE cursor stays active. As a hack, just move the scene visibility to the left by
        ;; 13 pixels, that seems to be enough to allow the cursor to get reset.
        controls (settings-popup/show! owner keymap localization scene-vis-store 230 -13.0 setting-descriptors nil
                                         (fn [_]
                                           (sync-filter-checkboxes! scene-visibility)
                                           ;; NOTE: On close, free the references to the GUI nodes
                                           (g/set-property! scene-visibility :ui-check-boxes nil)))]
    (when controls
      (g/update-property! scene-visibility :ui-check-boxes assoc
                          :visibility-filter-check-box (last (:visibility-filters-enabled? controls))
                          :component-guide-check-box (last (:outline controls))
                          :filter-check-boxes (coll/into-> controls []
                                                           (keep (fn [[key entry]]
                                                                   (when (contains? toggleable-filters key)
                                                                     ;; NOTE: We only need the HBox for this, so we can disable the whole thing
                                                                     (first entry))))))
      (sync-filter-checkboxes! scene-visibility))))

(defn toggle-tag-visibility! [scene-visibility tag]
  (g/update-property! scene-visibility :filtered-renderable-tags
                      (fn [tags]
                        (if (contains? tags tag)
                          (disj tags tag)
                          (conj tags tag)))))

(handler/defhandler :scene.visibility.toggle-filters :workbench
  (active? [scene-visibility evaluation-context]
    (g/node-value scene-visibility :active-scene-resource-node evaluation-context))
  (run [scene-visibility]
    (g/update-property! scene-visibility :visibility-filters-enabled? not)
    (sync-filter-checkboxes! scene-visibility)))

(handler/defhandler :scene.visibility.toggle-component-guides :workbench
  (active? [scene-visibility evaluation-context]
    (g/node-value scene-visibility :active-scene-resource-node evaluation-context))
  (run [scene-visibility]
    (toggle-tag-visibility! scene-visibility :outline)
    (sync-filter-checkboxes! scene-visibility)))

(handler/defhandler :scene.visibility.toggle-grid :workbench
  (active? [app-view scene-visibility evaluation-context]
    (and (g/node-value scene-visibility :active-scene-resource-node evaluation-context)
         (when-let [active-view (g/node-value app-view :active-view evaluation-context)]
           (some? (g/maybe-node-value active-view :grid evaluation-context)))))
  (run [scene-visibility] (toggle-tag-visibility! scene-visibility :grid))
  (state [scene-visibility evaluation-context]
    (not (:grid (g/node-value scene-visibility :filtered-renderable-tags evaluation-context)))))

(defn hidden-outline-key-path?
  [hidden-node-outline-key-paths node-outline-key-path]
  (boolean (some #(iutil/seq-starts-with? node-outline-key-path %)
                 hidden-node-outline-key-paths)))
