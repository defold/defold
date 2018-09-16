(ns editor.scene-visibility
  (:require [clojure.set :as set]
            [dynamo.graph :as g]
            [editor.keymap :as keymap]
            [editor.resource-node :as resource-node]
            [editor.ui :as ui]
            [editor.ui.popup :as popup]
            [editor.util :as util]
            [schema.core :as s])
  (:import [javafx.geometry Point2D Pos]
           [javafx.scene Parent]
           [javafx.scene.control CheckBox Label PopupControl]
           [javafx.scene.layout HBox Priority VBox]))

(set! *warn-on-reflection* true)

;; -----------------------------------------------------------------------------
;; Per-Object Visibility
;; -----------------------------------------------------------------------------

(defn node-outline-key-path? [value]
  (and (vector? value)
       (let [[resource-node & outline-node-names] value]
         (and (integer? resource-node)
              (every? string? outline-node-names)))))

(defn outline-name-path? [value]
  (and (vector? value)
       (every? string? value)))

(g/deftype NodeOutlineKeyPaths #{(s/pred node-outline-key-path?)})
(g/deftype HideHistory [(s/both #{(s/pred outline-name-path?)} (s/pred seq))])

(g/defnode SceneHideHistoryNode
  (property hide-history HideHistory)
  (input scene-resource-node g/NodeID)
  (output hidden-node-outline-key-paths NodeOutlineKeyPaths (g/fnk [hide-history scene-resource-node]
                                                              (into #{}
                                                                    (comp cat
                                                                          (map (partial into [scene-resource-node])))
                                                                    hide-history))))

(defn- find-scene-hide-history-node [toggles-node scene-resource-node]
  (some (fn [[scene-hide-history-node]]
          (when (some-> (g/node-feeding-into scene-hide-history-node :scene-resource-node) (= scene-resource-node))
            scene-hide-history-node))
        (g/sources-of toggles-node :hidden-node-outline-key-paths)))

(defn- active-scene-resource-node [toggles-node]
  (when-some [active-resource-node (g/node-value toggles-node :active-resource-node)]
    (when (g/has-output? (g/node-type* active-resource-node) :scene)
      active-resource-node)))

(defn remove-invalid-scene-hide-history-nodes! [toggles-node]
  (let [invalid-scene-hide-history-nodes (into []
                                               (keep (fn [[scene-hide-history-node]]
                                                       (let [scene-resource-node (g/node-feeding-into scene-hide-history-node :scene-resource-node)]
                                                         (when (or (nil? scene-resource-node)
                                                                   (resource-node/defective? scene-resource-node))
                                                           scene-hide-history-node))))
                                               (g/sources-of toggles-node :hidden-node-outline-key-paths))]
    (when (seq invalid-scene-hide-history-nodes)
      (g/transact
        (map g/delete-node invalid-scene-hide-history-nodes)))))

(defn hidden-node-outline-key-paths [toggles-node]
  (when-some [active-scene-resource-node (active-scene-resource-node toggles-node)]
    (when-some [scene-hide-history-node (find-scene-hide-history-node toggles-node active-scene-resource-node)]
      (not-empty (g/node-value scene-hide-history-node :hidden-node-outline-key-paths)))))

(defn last-hidden-node-outline-key-paths [toggles-node]
  (when-some [active-scene-resource-node (active-scene-resource-node toggles-node)]
    (when-some [scene-hide-history-node (find-scene-hide-history-node toggles-node active-scene-resource-node)]
      (when-some [outline-name-paths (peek (g/node-value scene-hide-history-node :hide-history))]
        (into #{}
              (map (partial into [active-scene-resource-node]))
              outline-name-paths)))))

(defn show-node-outline-key-paths! [toggles-node node-outline-key-paths]
  (assert (set? (not-empty node-outline-key-paths)))
  (assert (every? node-outline-key-path? node-outline-key-paths))
  (let [outline-name-paths-by-scene-hide-history-node (into {}
                                                            (map (fn [[scene-resource-node scene-node-outline-key-paths]]
                                                                   (let [scene-hide-history-node (find-scene-hide-history-node toggles-node scene-resource-node)
                                                                         outline-name-paths (into #{} (map (comp vec rest)) scene-node-outline-key-paths)]
                                                                     [scene-hide-history-node outline-name-paths])))
                                                            (group-by first node-outline-key-paths))]
    (g/transact
      (for [[scene-hide-history-node outline-name-paths] outline-name-paths-by-scene-hide-history-node]
        (g/update-property scene-hide-history-node :hide-history
                           (fn [hide-history]
                             ;; Removing the now-visible nodes from the history
                             ;; ensures the Show Last Hidden Objects command
                             ;; will work as expected if the user manually shows
                             ;; nodes she had previously hidden.
                             (into []
                                   (keep (fn [hidden-outline-name-paths]
                                           (not-empty (set/difference hidden-outline-name-paths outline-name-paths))))
                                   hide-history)))))

    ;; Remove SceneHideHistoryNodes whose history is now empty.
    (let [empty-scene-hide-history-nodes (keep (fn [[scene-hide-history-node]]
                                                 (when (empty? (g/node-value scene-hide-history-node :hide-history))
                                                   scene-hide-history-node))
                                               outline-name-paths-by-scene-hide-history-node)]
      (when (seq empty-scene-hide-history-nodes)
        (g/transact
          (map g/delete-node empty-scene-hide-history-nodes))))))

(defn hide-node-outline-key-paths! [toggles-node node-outline-key-paths]
  (assert (set? (not-empty node-outline-key-paths)))
  (assert (every? node-outline-key-path? node-outline-key-paths))
  (let [graph (g/node-id->graph-id toggles-node)]
    (g/transact
      (for [[scene-resource-node scene-node-outline-key-paths] (group-by first node-outline-key-paths)
            :let [scene-hide-history-node (find-scene-hide-history-node toggles-node scene-resource-node)
                  outline-name-paths (into #{} (map (comp vec rest)) scene-node-outline-key-paths)]]
        (if (some? scene-hide-history-node)
          (g/update-property scene-hide-history-node :hide-history conj outline-name-paths)
          (g/make-nodes graph [scene-hide-history-node [SceneHideHistoryNode :hide-history [outline-name-paths]]]
                        (g/connect scene-resource-node :_node-id scene-hide-history-node :scene-resource-node)
                        (g/connect scene-hide-history-node :hidden-node-outline-key-paths toggles-node :hidden-node-outline-key-paths)))))))

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

(defn set-tag-visibility! [toggles-node tag visible]
  (g/update-property! toggles-node :hidden-renderable-tags (if visible disj conj) tag))

(defn toggle-tag-visibility! [toggles-node tag]
  (g/update-property! toggles-node :hidden-renderable-tags (fn [tags]
                                                             (if (contains? tags tag)
                                                               (disj tags tag)
                                                               (conj tags tag)))))

(defn set-filters-enabled! [toggles-node enabled]
  (g/set-property! toggles-node :visibility-filters-enabled enabled))

(defn toggle-filters-enabled! [toggles-node]
  (g/update-property! toggles-node :visibility-filters-enabled not))

(defn- make-visibility-toggles-list ^VBox [toggles-node]
  (let [tag-toggles (map (fn [info]
                           (let [[toggle-box update-fn] (make-toggle {:label (:label info)
                                                                      :on-change (fn [checked] (set-tag-visibility! toggles-node (:tag info) checked))
                                                                      :acc ""})
                                 update-from-hidden-tags (fn [hidden-tags enabled] (update-fn (not (contains? hidden-tags (:tag info))) enabled))]
                             [toggle-box update-from-hidden-tags]))
                         tag-toggles-info)
        enabled-toggle (let [[^HBox toggle-box update-fn] (make-toggle {:label "Visibility Filters"
                                                                        :on-change (fn [checked] (set-filters-enabled! toggles-node checked))
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
                      (let [hidden-tags (g/node-value toggles-node :hidden-renderable-tags)
                            visibility-filters-enabled (g/node-value toggles-node :visibility-filters-enabled)]
                        ((second enabled-toggle) visibility-filters-enabled true)
                        (doseq [tag-toggle tag-toggles]
                          ((second tag-toggle) hidden-tags visibility-filters-enabled))))]
      [box update-fn])))

(defn- pref-popup-position
  ^Point2D [^Parent container width y-gap]
  (let [container-screen-bounds (.localToScreen container (.getBoundsInLocal container))]
    (Point2D. (- (.getMaxX container-screen-bounds) width 10)
              (+ (.getMaxY container-screen-bounds) y-gap))))

(defn show-visibility-settings [^Parent owner settings-node]
  (if-let [popup ^PopupControl (ui/user-data owner ::popup)]
    (.hide popup)
    (let [[^VBox toggles update-fn] (make-visibility-toggles-list settings-node)
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
