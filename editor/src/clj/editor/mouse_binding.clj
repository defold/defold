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

(ns editor.mouse-binding
  (:require [clojure.string :as string]
            [editor.input :as i]
            [editor.os :as os]))

(set! *warn-on-reflection* true)

(def buttons [:primary :middle :secondary])
(def modifiers [:shift :control :alt])

(def button->label
  {:primary "Left"
   :middle "Middle"
   :secondary "Right"})

(def modifier->label
  {:shift "Shift"
   :alt (if (os/is-mac-os?) "Opt" "Alt")
   :control "Ctrl"})

(def trigger->label
  {:drag "Drag"
   :press "Press"
   :click "Click"})

(defonce bindings-atom
  (atom {;; contexts: {context {command [mouse-binding]}}
         :contexts {}
         ;; overrides: {[context command] {:bindings [binding ...] :sub-commands {sub-cmd modifier}}}
         :overrides {}}))

(defn- add-bindings [state context context-path bindings]
  (assoc-in state
            [:contexts context]
            (group-by :command (mapv #(assoc % :context-path context-path) bindings))))

(defn- remove-bindings [state context]
  (update state :contexts dissoc context))

(defn register! [context context-path bindings]
  (swap! bindings-atom #(-> %
                            (remove-bindings context)
                            (add-bindings context context-path bindings)))
  nil)

(defn unregister! [context]
  (swap! bindings-atom remove-bindings context)
  nil)

(defn set-overrides! [overrides]
  (swap! bindings-atom assoc :overrides (or overrides {}))
  nil)

(defn- effective-command-bindings [state context command registered-bindings]
  (if-let [override-bindings (:bindings (get (:overrides state) [context command]))]
    (let [template (dissoc (first registered-bindings) :binding)]
      (mapv #(assoc (get registered-bindings %1 template) :binding %2)
            (range)
            override-bindings))
    registered-bindings))

(defn registered-command-bindings [context command]
  (get-in @bindings-atom [:contexts context command]))

(defn all-bindings []
  (let [state @bindings-atom]
    (into []
          (mapcat (fn [[context command->bindings]]
                    (map (fn [[command bindings]]
                           (let [effective (effective-command-bindings state context command bindings)
                                 template (dissoc (first bindings) :binding)]
                             (assoc template
                               :context context
                               :command command
                               :bindings (mapv :binding effective))))
                         command->bindings)))
          (:contexts state))))

(defn bindings [context]
  (let [state @bindings-atom]
    (into []
          (mapcat (fn [[command bindings]]
                    (effective-command-bindings state context command bindings)))
          (get-in state [:contexts context]))))

(defn command-active? [context command input-state]
  (let [state @bindings-atom]
    (boolean
      (some (fn [mouse-binding]
              (when-let [binding (:binding mouse-binding)]
                (i/mouse-binding-active? binding input-state)))
            (effective-command-bindings state context command (get-in state [:contexts context command]))))))

(defn command-for-action [context action]
  (let [state @bindings-atom]
    (some (fn [[command bindings]]
            (when (some (fn [mouse-binding]
                          (when-let [binding (:binding mouse-binding)]
                            (i/mouse-binding-action? binding action)))
                        (effective-command-bindings state context command bindings))
              command))
          (get-in state [:contexts context]))))

(defn sub-command-active? [context command sub-cmd input-state]
  (let [state @bindings-atom
        registered-sub-cmds (some :sub-commands (get-in state [:contexts context command]))
        default-modifier (some #(when (= (:command %) sub-cmd) (:modifier %)) registered-sub-cmds)
        effective-modifier (get-in state [:overrides [context command] :sub-commands sub-cmd] default-modifier)]
    (boolean (when effective-modifier
               (contains? (:modifiers input-state) effective-modifier)))))

(defn binding-display-text [{selected-modifiers :modifiers
                             :keys [button]}]
  (if button
    (let [parts (into []
                      (comp (filter (set selected-modifiers))
                            (map modifier->label))
                      modifiers)]
      (string/join "+" (conj parts (button->label button))))
    "Unassigned"))

(comment
  @bindings-atom
  (reset! bindings-atom {:contexts {}})
  (command-active? :editor.tile-map/tile-map
                   :scene.tile-map.erase
                   {:mouse-buttons #{:primary}
                    :pressed-keys #{}
                    :modifiers #{:shift :alt}
                    :cursor-pos [0.0 0.0]
                    :view-pos [0.0 0.0]
                    :scroll-delta [0.0 0.0]})
  (command-for-action :editor.tile-map/tile-map {:type :drag
                                                 :button :primary
                                                 :shift true
                                                 :alt true})
  :-)
