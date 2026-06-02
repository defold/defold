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
            [editor.input :as i]))

(set! *warn-on-reflection* true)

(def buttons [:primary :middle :secondary])
(def modifiers [:shift :alt :control])

(def button->label
  {:primary "Left"
   :middle "Middle"
   :secondary "Right"})

(def modifier->label
  {:shift "Shift"
   :alt "Alt/Opt"
   :control "Ctrl"})

(def trigger->label
  {:drag "Drag"
   :press "Press"
   :click "Click"})

(defonce bindings-atom
  (atom {;; contexts: {context {command [mouse-binding]}}
         :contexts {}}))

(defn- add-bindings [state context bindings]
  (assoc-in state [:contexts context] (group-by :command bindings)))

(defn- remove-bindings [state context]
  (update state :contexts dissoc context))

(defn register! [context bindings]
  (swap! bindings-atom #(-> %
                            (remove-bindings context)
                            (add-bindings context bindings)))
  nil)

(defn unregister! [context]
  (swap! bindings-atom remove-bindings context)
  nil)

(defn all-bindings []
  (into []
        (mapcat (fn [[context command->bindings]]
                  (mapcat (fn [[command bindings]]
                            (map-indexed (fn [binding-index mouse-binding]
                                           (assoc mouse-binding
                                             :context context
                                             :command command
                                             :binding-index binding-index))
                                         bindings))
                          command->bindings)))
        (:contexts @bindings-atom)))

(defn bindings [context]
  (into [] cat (vals (get-in @bindings-atom [:contexts context]))))

(defn command-active? [context command input-state]
  (boolean
    (some (fn [mouse-binding]
            (when-let [binding (:binding mouse-binding)]
              (i/mouse-binding-active? binding input-state)))
          (get-in @bindings-atom [:contexts context command]))))

(defn command-for-action [context action]
  (some (fn [[command bindings]]
          (when (some (fn [mouse-binding]
                        (when-let [binding (:binding mouse-binding)]
                          (i/mouse-binding-action? binding action)))
                      bindings)
            command))
        (get-in @bindings-atom [:contexts context])))

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
