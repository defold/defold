(ns editor.scene-visibility
  (:require [clojure.set :as set]
            [dynamo.graph :as g]
            [editor.handler :as handler]
            [editor.keymap :as keymap]
            [editor.system :as system]
            [editor.types :as types]
            [editor.ui :as ui]
            [editor.ui.popup :as popup]
            [editor.util :as util]
            [internal.util :as iutil]
            [schema.core :as s])
  (:import [javafx.geometry Insets Point2D Pos]
           [javafx.scene Parent]
           [javafx.scene.control CheckBox Label PopupControl Separator]
           [javafx.scene.layout HBox Priority Region StackPane VBox]))

(set! *warn-on-reflection* true)

(def ^:private renderable-tag-toggles-info
  (cond-> [{:label "Collision Shapes" :tag :collision-shape}
           #_{:label "GUI Elements" :tag :gui} ; This tag exists, but we decided to hide it and put in granular control instead. Add back if we make the toggles hierarchical?
           {:label "GUI Bounds" :tag :gui-bounds}
           {:label "GUI Shapes" :tag :gui-shape}
           {:label "GUI Particle Effects" :tag :gui-particlefx}
           {:label "GUI Spine Scenes" :tag :gui-spine}
           {:label "GUI Text" :tag :gui-text}
           {:label "Models" :tag :model}
           {:label "Particle Effects" :tag :particlefx}
           {:label "Skeletons" :tag :skeleton}
           {:label "Spine Scenes" :tag :spine}
           {:label "Sprites" :tag :sprite}
           {:label "Text" :tag :text}
           {:label "Tile Maps" :tag :tilemap}]

          (system/defold-dev?)
          (into [{:label :separator}
                 {:label "Scene Visibility Bounds" :tag :dev-visibility-bounds :appear-filtered false}])))

(def ^:private appear-filtered-renderable-tags
  (into #{}
        (keep (fn [{:keys [appear-filtered tag]
                    :or {appear-filtered true}}]
                (when appear-filtered
                  tag)))
        renderable-tag-toggles-info))

(defn filters-appear-active?
  "Returns true if some parts of the scene are hidden due to visibility filters.
  Does not consider scene elements that you'd not typically expect to be there,
  such as debug rendering of bounding volumes, etc."
  ([scene-visibility]
   (g/with-auto-evaluation-context evaluation-context
     (filters-appear-active? scene-visibility evaluation-context)))
  ([scene-visibility evaluation-context]
   (boolean
     (and (g/node-value scene-visibility :visibility-filters-enabled? evaluation-context)
          (some appear-filtered-renderable-tags
                (g/node-value scene-visibility :filtered-renderable-tags evaluation-context))))))

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
  (property visibility-filters-enabled? g/Bool (default true))
  (property filtered-renderable-tags types/RenderableTags (default #{:dev-visibility-bounds}))

  (input active-resource-node g/NodeID)
  (input active-scene g/Any :substitute nil)
  (input outline-selection g/Any :substitute nil)
  (input scene-hide-history-datas SceneHideHistoryData :array)

  (output active-scene-resource-node g/NodeID (g/fnk [_basis active-resource-node]
                                                (when (some? active-resource-node)
                                                  (when (g/has-output? (g/node-type* _basis active-resource-node) :scene)
                                                    active-resource-node))))

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

  (output outline-name-paths-by-selection-state OutlineNamePathsByBool :cached (g/fnk [active-scene outline-selection]
                                                                                 (let [selected-outline-name-paths (into [] (keep outline-selection-entry->outline-name-path) outline-selection)
                                                                                       outline-name-path-below-selection? (fn [outline-name-path]
                                                                                                                            (boolean (some #(iutil/seq-starts-with? outline-name-path %)
                                                                                                                                           selected-outline-name-paths)))]
                                                                                   (iutil/group-into {} #{}
                                                                                                     outline-name-path-below-selection?
                                                                                                     (scene-outline-name-paths active-scene)))))

  (output selected-outline-name-paths OutlineNamePaths (g/fnk [outline-name-paths-by-selection-state]
                                                         (outline-name-paths-by-selection-state true)))

  (output unselected-outline-name-paths OutlineNamePaths (g/fnk [outline-name-paths-by-selection-state]
                                                           (outline-name-paths-by-selection-state false)))

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

(defn make-scene-visibility-node! [view-graph]
  (g/make-node! view-graph SceneVisibilityNode))

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

(handler/defhandler :hide-unselected :workbench
  (active? [scene-visibility evaluation-context]
           (g/node-value scene-visibility :active-scene-resource-node evaluation-context))
  (enabled? [scene-visibility evaluation-context]
            (g/node-value scene-visibility :unselected-hideable-outline-name-paths evaluation-context))
  (run [scene-visibility] (hide-outline-name-paths! scene-visibility (g/node-value scene-visibility :unselected-hideable-outline-name-paths))))

(handler/defhandler :hide-selected :workbench
  (active? [scene-visibility evaluation-context]
           (g/node-value scene-visibility :active-scene-resource-node evaluation-context))
  (enabled? [scene-visibility evaluation-context]
            (g/node-value scene-visibility :selected-hideable-outline-name-paths evaluation-context))
  (run [scene-visibility] (hide-outline-name-paths! scene-visibility (g/node-value scene-visibility :selected-hideable-outline-name-paths))))

(handler/defhandler :show-selected :workbench
  (active? [scene-visibility evaluation-context]
           (g/node-value scene-visibility :active-scene-resource-node evaluation-context))
  (enabled? [scene-visibility evaluation-context]
            (g/node-value scene-visibility :selected-showable-outline-name-paths evaluation-context))
  (run [scene-visibility] (show-outline-name-paths! scene-visibility (g/node-value scene-visibility :selected-showable-outline-name-paths))))

(handler/defhandler :show-last-hidden :workbench
  (active? [scene-visibility evaluation-context]
           (g/node-value scene-visibility :active-scene-resource-node evaluation-context))
  (enabled? [scene-visibility evaluation-context]
            (g/node-value scene-visibility :last-hidden-outline-name-paths evaluation-context))
  (run [scene-visibility] (show-outline-name-paths! scene-visibility (g/node-value scene-visibility :last-hidden-outline-name-paths))))

(handler/defhandler :show-all-hidden :workbench
  (active? [scene-visibility evaluation-context]
           (g/node-value scene-visibility :active-scene-resource-node evaluation-context))
  (enabled? [scene-visibility evaluation-context]
            (g/node-value scene-visibility :hidden-outline-name-paths evaluation-context))
  (run [scene-visibility] (show-outline-name-paths! scene-visibility (g/node-value scene-visibility :hidden-outline-name-paths))))

;; -----------------------------------------------------------------------------
;; Visibility Filters
;; -----------------------------------------------------------------------------

(defn- make-toggle [{:keys [label acc on-change]}]
  (let [check-box (CheckBox.)
        label (Label. label)
        acc (Label. acc)]
    (ui/on-action! check-box (fn [_] (on-change (ui/value check-box))))
    (ui/remove-style! check-box "check-box")
    (ui/add-style! check-box "slide-switch")
    (HBox/setHgrow label Priority/ALWAYS)
    (ui/add-style! label "slide-switch-label")
    (when (util/is-mac-os?)
      (.setStyle acc "-fx-font-family: 'Lucida Grande';"))
    (ui/add-style! acc "accelerator-label")
    (let [hbox (doto (HBox.)
                 (.setAlignment Pos/CENTER_LEFT)
                 (ui/on-click! (fn [_]
                                 (ui/value! check-box (not (ui/value check-box)))
                                 (on-change (ui/value check-box))))
                 (ui/children! [check-box label acc]))
          update (fn [checked enabled]
                   (ui/enable! hbox enabled)
                   (ui/value! check-box checked))]
      [hbox update])))

(defn set-tag-visibility! [scene-visibility tag visible]
  (g/update-property! scene-visibility :filtered-renderable-tags (if visible disj conj) tag))

(defn toggle-tag-visibility! [scene-visibility tag]
  (g/update-property! scene-visibility :filtered-renderable-tags (fn [tags]
                                                                   (if (contains? tags tag)
                                                                     (disj tags tag)
                                                                     (conj tags tag)))))

(defn set-filters-enabled! [scene-visibility enabled]
  (g/set-property! scene-visibility :visibility-filters-enabled? enabled))

(defn toggle-filters-enabled! [scene-visibility]
  (g/update-property! scene-visibility :visibility-filters-enabled? not))

(defn- make-visibility-toggles-list
  ^Region [scene-visibility]
  (let [make-control
        (fn [{:keys [label tag]}]
          (if (= :separator label)
            [(Separator.) nil]
            (let [[control update-fn]
                  (make-toggle {:label label
                                :acc ""
                                :on-change (fn [checked]
                                             (set-tag-visibility! scene-visibility tag checked))})
                  update-from-hidden-tags
                  (fn [hidden-tags enabled]
                    (let [checked (not (contains? hidden-tags tag))]
                      (update-fn checked enabled)))]
              [control update-from-hidden-tags])))

        tag-toggles (mapv make-control renderable-tag-toggles-info)
        tag-toggle-update-fns (into [] (keep second) tag-toggles)

        update-tag-toggles
        (fn [hidden-tags enabled]
          (doseq [update-fn tag-toggle-update-fns]
            (update-fn hidden-tags enabled)))

        [filters-enabled-control filters-enabled-update-fn]
        (make-toggle {:label "Visibility Filters"
                      :acc (keymap/key-combo->display-text "Shift+Shortcut+I")
                      :on-change (fn [checked]
                                   (set-filters-enabled! scene-visibility checked))})

        container (doto (StackPane.)
                    (.setMinWidth 230)
                    (ui/children! [(doto (Region.)
                                     ;; Move drop shadow down a bit so it does not interfere with toolbar clicks.
                                     (StackPane/setMargin (Insets. 16.0 0.0 0.0 0.0))
                                     (ui/add-style! "visibility-toggles-shadow"))
                                   (doto (VBox.)
                                     (ui/add-style! "visibility-toggles-list")
                                     (ui/children! (into [filters-enabled-control]
                                                         (map first)
                                                         tag-toggles)))]))

        update-fn (fn []
                    (let [filtered-tags (g/node-value scene-visibility :filtered-renderable-tags)
                          visibility-filters-enabled? (g/node-value scene-visibility :visibility-filters-enabled?)]
                      (filters-enabled-update-fn visibility-filters-enabled? true)
                      (update-tag-toggles filtered-tags visibility-filters-enabled?)))]
    (ui/add-style! filters-enabled-control "first-entry")
    [container update-fn]))

(defn- pref-popup-position
  ^Point2D [^Parent container width y-gap]
  (let [container-screen-bounds (.localToScreen container (.getBoundsInLocal container))]
    (Point2D. (- (.getMaxX container-screen-bounds) width 10)
              (+ (.getMaxY container-screen-bounds) y-gap))))

(defn show-visibility-settings! [^Parent owner scene-visibility]
  (if-let [popup ^PopupControl (ui/user-data owner ::popup)]
    (.hide popup)
    (let [[^Region toggles update-fn] (make-visibility-toggles-list scene-visibility)
          popup (popup/make-popup owner toggles)
          anchor (pref-popup-position owner (.getMinWidth toggles) -5)
          refresh-timer (ui/->timer 13 "refresh-tag-filters" (fn [_ _] (update-fn)))]
      (update-fn)
      (ui/user-data! owner ::popup popup)
      (ui/on-closed! popup (fn [_] (ui/user-data! owner ::popup nil)))
      (ui/timer-stop-on-closed! popup refresh-timer)
      (ui/timer-start! refresh-timer)
      (.show popup owner (.getX anchor) (.getY anchor)))))
  
(defn settings-visible? [^Parent owner]
  (some? (ui/user-data owner ::popup)))

(handler/defhandler :toggle-visibility-filters :workbench
  (active? [scene-visibility evaluation-context]
           (g/node-value scene-visibility :active-scene-resource-node evaluation-context))
  (run [scene-visibility] (toggle-filters-enabled! scene-visibility)))

(handler/defhandler :toggle-component-guides :workbench
  (active? [scene-visibility evaluation-context]
           (g/node-value scene-visibility :active-scene-resource-node evaluation-context))
  (run [scene-visibility] (toggle-tag-visibility! scene-visibility :outline)))

(handler/defhandler :toggle-grid :workbench
  (active? [scene-visibility evaluation-context]
           (g/node-value scene-visibility :active-scene-resource-node evaluation-context))
  (run [scene-visibility] (toggle-tag-visibility! scene-visibility :grid)))
