(ns editor.scene-visibility
  (:require [clojure.set :as set]
            [dynamo.graph :as g]
            [editor.handler :as handler]
            [editor.keymap :as keymap]
            [editor.types :as types]
            [editor.ui :as ui]
            [editor.ui.popup :as popup]
            [editor.util :as util]
            [internal.util :as iutil]
            [schema.core :as s])
  (:import [javafx.geometry Point2D Pos]
           [javafx.scene Parent]
           [javafx.scene.control CheckBox Label PopupControl]
           [javafx.scene.layout HBox Priority VBox]))

(set! *warn-on-reflection* true)

;; -----------------------------------------------------------------------------
;; SceneVisibilityNode
;; -----------------------------------------------------------------------------

(defn outline-name-path? [value]
  (and (vector? value)
       (every? string? value)))

(def ^:private TOutlineNamePaths #{(s/pred outline-name-path?)})
(def ^:private THideHistory [(s/both TOutlineNamePaths (s/pred seq))])
(g/deftype HideHistory THideHistory)
(g/deftype HideHistoryByNodeID {s/Int THideHistory})
(g/deftype NodeIDAndHideHistory [(s/one s/Int "node-id") (s/one THideHistory "hide-history")])
(g/deftype OutlineNamePaths TOutlineNamePaths)
(g/deftype OutlineNamePathsByNodeID {s/Int (s/both TOutlineNamePaths (s/pred seq))})

(defn- scene-outline-name-paths
  ([scene]
   (scene-outline-name-paths [] scene))
  ([outline-name-path {:keys [node-id children] :as _scene}]
   (mapcat (fn [{child-node-id :node-id child-node-outline-key :node-outline-key :as child-scene}]
             (if (= node-id child-node-id)
               (scene-outline-name-paths outline-name-path child-scene)
               (let [child-outline-name-path (conj outline-name-path child-node-outline-key)]
                 (cons child-outline-name-path
                       (scene-outline-name-paths child-outline-name-path child-scene)))))
           children)))

(def ^:private outline-selection-entry->outline-name-path (comp not-empty vec next :node-outline-key-path))

(g/defnode SceneVisibilityNode
  (property visibility-filters-enabled? g/Bool (default true))
  (property filtered-renderable-tags types/RenderableTags (default #{}))

  (input active-resource-node g/NodeID)
  (input outline-selection g/Any)
  (input scene g/Any)
  (input scene-resource-node+hide-historys NodeIDAndHideHistory :array)

  (output scene-resource-node g/NodeID (g/fnk [_basis active-resource-node]
                                         (when (some? active-resource-node)
                                           (when (g/has-output? (g/node-type* _basis active-resource-node) :scene)
                                             active-resource-node))))

  (output hide-history-by-scene-resource-node HideHistoryByNodeID :cached (g/fnk [scene-resource-node+hide-historys]
                                                                            (into {} scene-resource-node+hide-historys)))

  (output hidden-outline-name-paths-by-scene-resource-node OutlineNamePathsByNodeID :cached (g/fnk [hide-history-by-scene-resource-node]
                                                                                              (into {}
                                                                                                    (keep (fn [[scene-resource-node hide-history]]
                                                                                                            (when-some [hidden-outline-name-paths (not-empty (apply set/union hide-history))]
                                                                                                              [scene-resource-node hidden-outline-name-paths])))
                                                                                                    hide-history-by-scene-resource-node)))

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

  (output hidden-outline-name-paths OutlineNamePaths (g/fnk [hidden-outline-name-paths-by-scene-resource-node scene-resource-node]
                                                       (hidden-outline-name-paths-by-scene-resource-node scene-resource-node)))

  (output selected-outline-name-paths OutlineNamePaths :cached (g/fnk [outline-selection scene]
                                                                 (let [selected-outline-name-paths (into [] (keep outline-selection-entry->outline-name-path) outline-selection)
                                                                       outline-name-path-below-selection? (fn [outline-name-path]
                                                                                                            (boolean (some #(iutil/seq-starts-with? outline-name-path %)
                                                                                                                           selected-outline-name-paths)))]
                                                                   (into #{}
                                                                         (filter outline-name-path-below-selection?)
                                                                         (scene-outline-name-paths scene)))))

  (output hideable-outline-name-paths OutlineNamePaths :cached (g/fnk [hidden-outline-name-paths selected-outline-name-paths]
                                                                 (not-empty (set/difference selected-outline-name-paths hidden-outline-name-paths))))

  (output showable-outline-name-paths OutlineNamePaths :cached (g/fnk [hidden-outline-name-paths selected-outline-name-paths]
                                                                 (not-empty (set/intersection selected-outline-name-paths hidden-outline-name-paths))))

  (output last-hidden-outline-name-paths OutlineNamePaths (g/fnk [hide-history-by-scene-resource-node scene-resource-node]
                                                            (peek (hide-history-by-scene-resource-node scene-resource-node)))))

(defn make-scene-visibility-node! [view-graph]
  (g/make-node! view-graph SceneVisibilityNode))

;; -----------------------------------------------------------------------------
;; Per-Object Visibility
;; -----------------------------------------------------------------------------

(g/defnode SceneHideHistoryNode
  (property hide-history HideHistory)
  (input scene-resource-node g/NodeID)
  (output scene-resource-node+hide-history NodeIDAndHideHistory (g/fnk [hide-history scene-resource-node]
                                                                  [scene-resource-node hide-history])))

(defn- find-scene-hide-history-node [scene-visibility scene-resource-node]
  (some (fn [[scene-hide-history-node]]
          (when (some-> (g/node-feeding-into scene-hide-history-node :scene-resource-node) (= scene-resource-node))
            scene-hide-history-node))
        (g/sources-of scene-visibility :scene-resource-node+hide-historys)))

(defn- show-outline-name-paths! [scene-visibility outline-name-paths]
  (assert (set? (not-empty outline-name-paths)))
  (assert (every? outline-name-path? outline-name-paths))
  (let [scene-resource-node (g/node-value scene-visibility :scene-resource-node)
        scene-hide-history-node (find-scene-hide-history-node scene-visibility scene-resource-node)]

    ;; Remove the now-visible nodes from the hide history. This ensures the Show
    ;; Last Hidden Objects command works as expected if the user manually shows
    ;; nodes whe has previously hidden.
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
  (let [scene-resource-node (g/node-value scene-visibility :scene-resource-node)
        scene-hide-history-node (find-scene-hide-history-node scene-visibility scene-resource-node)]
    (if (some? scene-hide-history-node)
      (g/update-property! scene-hide-history-node :hide-history conj outline-name-paths)
      (g/transact
        (g/make-nodes (g/node-id->graph-id scene-visibility)
                      [scene-hide-history-node [SceneHideHistoryNode :hide-history [outline-name-paths]]]
                      (g/connect scene-resource-node :_node-id scene-hide-history-node :scene-resource-node)
                      (g/connect scene-hide-history-node :scene-resource-node+hide-history scene-visibility :scene-resource-node+hide-historys))))))

(handler/defhandler :hide-selected :workbench
  (active? [scene-visibility] (g/node-value scene-visibility :scene-resource-node))
  (enabled? [scene-visibility] (g/node-value scene-visibility :hideable-outline-name-paths))
  (run [scene-visibility] (hide-outline-name-paths! scene-visibility (g/node-value scene-visibility :hideable-outline-name-paths))))

(handler/defhandler :show-selected :workbench
  (active? [scene-visibility] (g/node-value scene-visibility :scene-resource-node))
  (enabled? [scene-visibility] (g/node-value scene-visibility :showable-outline-name-paths))
  (run [scene-visibility] (show-outline-name-paths! scene-visibility (g/node-value scene-visibility :showable-outline-name-paths))))

(handler/defhandler :show-last-hidden :workbench
  (active? [scene-visibility] (g/node-value scene-visibility :scene-resource-node))
  (enabled? [scene-visibility] (g/node-value scene-visibility :last-hidden-outline-name-paths))
  (run [scene-visibility] (show-outline-name-paths! scene-visibility (g/node-value scene-visibility :last-hidden-outline-name-paths))))

(handler/defhandler :show-all-hidden :workbench
  (active? [scene-visibility] (g/node-value scene-visibility :scene-resource-node))
  (enabled? [scene-visibility] (g/node-value scene-visibility :hidden-outline-name-paths))
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

(def ^:private tag-toggles-info
  [{:label "Collision Shapes" :tag :collision-shape}
   #_{:label "GUI Elements" :tag :gui}
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
   {:label "Tile Maps" :tag :tilemap}])

(def toggleable-tags (into #{} (map :tag) tag-toggles-info))

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

(defn- make-visibility-toggles-list ^VBox [scene-visibility]
  (let [tag-toggles (map (fn [info]
                           (let [[toggle-box update-fn] (make-toggle {:label (:label info)
                                                                      :on-change (fn [checked] (set-tag-visibility! scene-visibility (:tag info) checked))
                                                                      :acc ""})
                                 update-from-hidden-tags (fn [hidden-tags enabled] (update-fn (not (contains? hidden-tags (:tag info))) enabled))]
                             [toggle-box update-from-hidden-tags]))
                         tag-toggles-info)
        enabled-toggle (let [[^HBox toggle-box update-fn] (make-toggle {:label "Visibility Filters"
                                                                        :on-change (fn [checked] (set-filters-enabled! scene-visibility checked))
                                                                        :acc (keymap/key-combo->display-text "Shift+Shortcut+I")})]
                         (ui/add-style! toggle-box "first-entry")
                         [toggle-box update-fn])
        toggle-boxes (into [(first enabled-toggle)]
                           (map first)
                           tag-toggles)]
    (let [box (doto (VBox.)
                (ui/add-style! "visibility-toggles-list")
                (.setMinWidth 230)
                (ui/children! toggle-boxes))
          update-fn (fn []
                      (let [filtered-tags (g/node-value scene-visibility :filtered-renderable-tags)
                            visibility-filters-enabled? (g/node-value scene-visibility :visibility-filters-enabled?)]
                        ((second enabled-toggle) visibility-filters-enabled? true)
                        (doseq [tag-toggle tag-toggles]
                          ((second tag-toggle) filtered-tags visibility-filters-enabled?))))]
      [box update-fn])))

(defn- pref-popup-position
  ^Point2D [^Parent container width y-gap]
  (let [container-screen-bounds (.localToScreen container (.getBoundsInLocal container))]
    (Point2D. (- (.getMaxX container-screen-bounds) width 10)
              (+ (.getMaxY container-screen-bounds) y-gap))))

(defn show-visibility-settings! [^Parent owner scene-visibility]
  (if-let [popup ^PopupControl (ui/user-data owner ::popup)]
    (.hide popup)
    (let [[^VBox toggles update-fn] (make-visibility-toggles-list scene-visibility)
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
  (active? [scene-visibility] (g/node-value scene-visibility :scene-resource-node))
  (run [scene-visibility] (toggle-filters-enabled! scene-visibility)))

(handler/defhandler :toggle-component-guides :workbench
  (active? [scene-visibility] (g/node-value scene-visibility :scene-resource-node))
  (run [scene-visibility] (toggle-tag-visibility! scene-visibility :outline)))

(handler/defhandler :toggle-grid :workbench
  (active? [scene-visibility] (g/node-value scene-visibility :scene-resource-node))
  (run [scene-visibility] (toggle-tag-visibility! scene-visibility :grid)))
