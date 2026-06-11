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
            [editor.os :as os]
            [editor.util :as util]))

(set! *warn-on-reflection* true)

(def buttons [:primary :middle :secondary])
(def modifiers [:shift :control :alt])

(def button->label
  {:primary "Left"
   :middle "Middle"
   :secondary "Right"})

(def modifier->label
  (if (os/is-mac-os?)
    {:shift "⇧"
     :alt "⌥"
     :control "⌃"}
    {:shift "Shift"
     :alt "Alt"
     :control "Ctrl"}))

(defonce bindings-atom
  (atom {;; contexts: {context {:bindings {command [mouse-binding]} ; see `register!` for `mouse-binding`
         ;;                     :fallback-context context
         ;;                     :overrides {command {:bindings [binding ...]
         ;;                                          :sub-commands {sub-cmd modifier}}}}}
         :contexts {}}))

(defn register!
  "Registers mouse bindings for a context.

  `context` - keyword identifying the context (e.g. `:scene`)
  `context-path` - display path shown in the preferences UI
  `bindings` - sequence of binding maps, each with:
    `:command` - keyword identifying the command
    `:binding` - map with `:button` and `:modifiers` keys
    `:sub-commands` - optional sequence of maps with `:command` and `:modifier`
                      keys, defining modifier keys that activate sub-commands
  `opts` - optional map with:
    `:fallback-context` - context to fall back to when no binding is found"
  ([context context-path bindings]
   (register! context context-path bindings nil))
  ([context context-path bindings opts]
   (swap! bindings-atom
          (fn [state]
            (let [state' (assoc-in state [:contexts context :bindings]
                                   (group-by :command
                                             (mapv #(assoc % :context-path context-path) bindings)))]
              (if (:fallback-context opts)
                (assoc-in state' [:contexts context :fallback-context] (:fallback-context opts))
                (update-in state' [:contexts context] dissoc :fallback-context)))))
   nil))

(defn unregister!
  "Removes all registered data for `context`, including bindings,
  fallback context, and overrides."
  [context]
  (swap! bindings-atom update :contexts dissoc context)
  nil)

(defn fallback-context [context]
  (get-in @bindings-atom [:contexts context :fallback-context]))

(defn set-overrides!
  "Replaces the per-context overrides in the bindings atom.

  `overrides` shape:
    {context {command {:bindings [binding ...]
                       :sub-commands {sub-cmd modifier}}}}"
  [overrides]
  (swap! bindings-atom update :contexts
         (fn [contexts]
           (let [all-ctxs (into (set (keys contexts)) (keys overrides))]
             (reduce (fn [acc ctx]
                       (let [ctx-data (get contexts ctx {})
                             ctx-overrides (get overrides ctx)]
                         (assoc acc ctx
                                (if ctx-overrides
                                  (assoc ctx-data :overrides ctx-overrides)
                                  (dissoc ctx-data :overrides)))))
                     {}
                     all-ctxs))))
  nil)

(defn- effective-command-bindings* [state context command registered-bindings]
  (let [override (get-in state [:contexts context :overrides command])]
    (if (contains? override :bindings)
      (let [override-bindings (:bindings override)
            template (dissoc (first registered-bindings) :binding)]
        (mapv #(assoc (get registered-bindings %1 template) :binding %2)
              (range)
              override-bindings))
      registered-bindings)))

(defn registered-command-bindings [context command]
  (get-in @bindings-atom [:contexts context :bindings command]))

(defn- command-bindings-override [overrides context command]
  (let [override (get-in overrides [context command])]
    (when (contains? override :bindings)
      (:bindings override))))

(defn command-bindings-for-edit [overrides context command]
  (or (command-bindings-override overrides context command)
      (into [] (keep :binding) (registered-command-bindings context command))))

(defn command-row [overrides context command]
  (let [binding-override (get-in overrides [context command])
        override-bindings (command-bindings-override overrides context command)
        registered (mapv :binding (registered-command-bindings context command))
        has-direct-binding (or (some :button override-bindings) (some :button registered))
        fallback-ctx (when-not has-direct-binding
                       (fallback-context context))
        fallback-registered (when fallback-ctx
                              (mapv :binding (registered-command-bindings fallback-ctx command)))
        fallback-override-bindings (when fallback-ctx
                                     (command-bindings-override overrides fallback-ctx command))
        inherited-bindings (when fallback-ctx
                             (let [bindings (or fallback-override-bindings fallback-registered)]
                               (when (some :button bindings)
                                 bindings)))
        fallback-context-path (when inherited-bindings
                                (some :context-path (registered-command-bindings fallback-ctx command)))
        template (some-> (first (registered-command-bindings context command))
                         (dissoc :binding))]
    (assoc template
      :kind :mouse-binding
      :context context
      :command command
      :binding-source (cond
                        (contains? binding-override :bindings) :custom
                        inherited-bindings :inherited
                        :else :default)
      :fallback-context-path fallback-context-path
      :bindings (filterv :button (cond
                                   (contains? binding-override :bindings) override-bindings
                                   inherited-bindings inherited-bindings
                                   :else registered)))))

(defn effective-sub-command-modifier [overrides context command sub-cmd]
  (let [registered-sub-cmds (some :sub-commands (registered-command-bindings context command))
        default-modifier (some #(when (= (:command %) sub-cmd) (:modifier %)) registered-sub-cmds)]
    (get-in overrides [context command :sub-commands sub-cmd] default-modifier)))

(defn- assoc-command-override [overrides context command override]
  (if (and (nil? (:bindings override)) (empty? (:sub-commands override)))
    (util/dissoc-in overrides [context command])
    (assoc-in overrides [context command] override)))

(defn update-command-bindings [overrides context command bindings]
  (let [registered (mapv :binding (registered-command-bindings context command))
        override (get-in overrides [context command] {})
        new-override (cond-> override
                       (= bindings registered) (dissoc :bindings)
                       (not= bindings registered) (assoc :bindings bindings))]
    (assoc-command-override overrides context command new-override)))

(defn remove-command-binding [overrides context command binding-index]
  (let [bindings (command-bindings-for-edit overrides context command)
        new-bindings (into [] (keep-indexed #(when (not= %1 binding-index) %2)) bindings)]
    (update-command-bindings overrides context command new-bindings)))

(defn reset-command-bindings [overrides context command]
  (assoc-command-override overrides context command (dissoc (get-in overrides [context command] {}) :bindings)))

(defn update-sub-command-modifier [overrides context command sub-cmd modifier]
  (let [registered-sub-cmds (some :sub-commands (registered-command-bindings context command))
        default-modifier (some #(when (= (:command %) sub-cmd) (:modifier %)) registered-sub-cmds)
        override (get-in overrides [context command] {})
        new-override (if (= modifier default-modifier)
                       (update override :sub-commands dissoc sub-cmd)
                       (assoc-in override [:sub-commands sub-cmd] modifier))
        new-override (cond-> new-override
                       (empty? (:sub-commands new-override)) (dissoc :sub-commands))]
    (assoc-command-override overrides context command new-override)))

(defn reset-sub-command-modifier [overrides context command sub-cmd]
  (update-sub-command-modifier overrides context command sub-cmd
                               (effective-sub-command-modifier {} context command sub-cmd)))

(defn all-bindings []
  (let [state @bindings-atom]
    (into []
          (mapcat (fn [[context {:keys [bindings]}]]
                    (map (fn [[command bindings]]
                           (let [effective (effective-command-bindings* state context command bindings)
                                 template (dissoc (first bindings) :binding)]
                             (assoc template
                               :context context
                               :command command
                               :bindings (mapv :binding effective))))
                         bindings)))
          (:contexts state))))

(defn bindings [context]
  (let [state @bindings-atom]
    (into []
          (mapcat (fn [[command bindings]]
                    (effective-command-bindings* state context command bindings)))
          (get-in state [:contexts context :bindings]))))

(defn- command-active?* [state context command input-state]
  (or (boolean
        (some (fn [mouse-binding]
                (when-let [binding (:binding mouse-binding)]
                  (i/mouse-binding-active? binding input-state)))
              (effective-command-bindings* state context command (get-in state [:contexts context :bindings command]))))
      (when-let [fallback (get-in state [:contexts context :fallback-context])]
        (command-active?* state fallback command input-state))))

(defn command-active? [context command input-state]
  (boolean (command-active?* @bindings-atom context command input-state)))

(defn- command-for-action* [state context action]
  (or (some (fn [[command bindings]]
              (when (some (fn [mouse-binding]
                            (when-let [binding (:binding mouse-binding)]
                              (i/mouse-binding-action? binding action)))
                          (effective-command-bindings* state context command bindings))
                command))
            (get-in state [:contexts context :bindings]))
      (when-let [fallback (get-in state [:contexts context :fallback-context])]
        (command-for-action* state fallback action))))

(defn command-for-action [context action]
  (command-for-action* @bindings-atom context action))

(defn sub-command-active? [context command sub-cmd input-state]
  (let [state @bindings-atom
        effective-modifier (get-in state [:contexts context :overrides command :sub-commands sub-cmd]
                                  (effective-sub-command-modifier {} context command sub-cmd))]
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
    ""))
