(ns editor.scene-visibility
  (:require [dynamo.graph :as g]
            [editor.keymap :as keymap]
            [editor.ui :as ui]
            [editor.util :as util]
            [editor.ui.popup :as popup])
  (:import [javafx.scene Parent Node]
           [javafx.scene.control CheckBox Label]
           [javafx.scene.layout VBox HBox Priority]
           [javafx.geometry Point2D HPos VPos Pos]
           [javafx.css Styleable]
           [javafx.scene.control PopupControl Skin]))

(set! *warn-on-reflection* true)

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
  [{:label "GUI Elements" :tag :gui}
   {:label "Models" :tag :model}
   {:label "Particle Effects" :tag :particlefx}
   {:label "Physics" :tag :collision-shape}
   {:label "Spine Scenes" :tag :spine}
   {:label "Sprites" :tag :sprite}
   {:label "Text" :tag :label}
   {:label "Tile Maps" :tag :tilemap}])

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
