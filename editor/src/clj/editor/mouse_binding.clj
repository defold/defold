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
         ;;                     :user-overrides {command {:bindings [binding ...]
         ;;                                                     :sub-commands {sub-cmd modifier}}}}}
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
            (cond-> (assoc-in state [:contexts context :bindings]
                              (group-by :command
                                        (mapv #(assoc % :context-path context-path) bindings)))
              (:fallback-context opts)
              (assoc-in [:contexts context :fallback-context] (:fallback-context opts)))))
   nil))

(defn unregister!
  "Removes all registered data for `context`, including bindings,
  fallback context, and user overrides."
  [context]
  (swap! bindings-atom update :contexts dissoc context)
  nil)

(defn fallback-context [context]
  (get-in @bindings-atom [:contexts context :fallback-context]))

(defn set-user-overrides!
  "Replaces the per-context user overrides in the bindings atom.

  `user-overrides` shape:
    {context {command {:bindings [binding ...]
                       :sub-commands {sub-cmd modifier}}}}"
  [user-overrides]
  (swap! bindings-atom update :contexts
         (fn [contexts]
           (let [all-ctxs (into (set (keys contexts)) (keys user-overrides))]
             (reduce (fn [acc ctx]
                       (let [ctx-data (get contexts ctx {})
                             ctx-user-overrides (get user-overrides ctx)]
                         (assoc acc ctx
                                (if ctx-user-overrides
                                  (assoc ctx-data :user-overrides ctx-user-overrides)
                                  (dissoc ctx-data :user-overrides)))))
                     {}
                     all-ctxs))))
  nil)

(defn- effective-command-bindings* [state context command default-bindings]
  (let [command-user-overrides (get-in state [:contexts context :user-overrides command])]
    (if (contains? command-user-overrides :bindings)
      (let [user-bindings (:bindings command-user-overrides)
            template (dissoc (first default-bindings) :binding)]
        (mapv #(assoc (get default-bindings %1 template) :binding %2)
              (range)
              user-bindings))
      default-bindings)))

(defn default-command-bindings [context command]
  (get-in @bindings-atom [:contexts context :bindings command]))

(defn- user-command-bindings [user-overrides context command]
  (let [command-user-overrides (get-in user-overrides [context command])]
    (when (contains? command-user-overrides :bindings)
      (:bindings command-user-overrides))))

(defn command-bindings-for-edit
  "Returns the flat binding vector the edit UI works with: user overrides if
  present, otherwise the registered defaults stripped to raw `{:button :modifiers}`
  maps. Does not apply fallback-context inheritance."
  [user-overrides context command]
  (or (user-command-bindings user-overrides context command)
      (into [] (keep :binding) (default-command-bindings context command))))

(defn command-row
  "Builds a prefs-UI row descriptor for a command. Returns a map with:
    `:binding-source` - `:custom` if the user has overrides, `:inherited` if
                        bindings come from a fallback context, `:default` otherwise
    `:fallback-context-path` - display path of the context being inherited from, or nil
    `:bindings`  - effective binding list shown in the UI
  Plus all template keys from the registered binding (`:context-path`, etc.)."
  [user-overrides context command]
  (let [command-user-overrides (get-in user-overrides [context command])
        user-bindings (user-command-bindings user-overrides context command)
        default-bindings (mapv :binding (default-command-bindings context command))
        has-direct-binding (or (some :button user-bindings) (some :button default-bindings))
        fallback-ctx (when-not has-direct-binding
                       (fallback-context context))
        fallback-default-bindings (when fallback-ctx
                                    (mapv :binding (default-command-bindings fallback-ctx command)))
        fallback-user-bindings (when fallback-ctx
                                 (user-command-bindings user-overrides fallback-ctx command))
        inherited-bindings (when fallback-ctx
                             (let [bindings (or fallback-user-bindings fallback-default-bindings)]
                               (when (some :button bindings)
                                 bindings)))
        fallback-context-path (when inherited-bindings
                                (some :context-path (default-command-bindings fallback-ctx command)))
        template (some-> (first (default-command-bindings context command))
                         (dissoc :binding))]
    (assoc template
      :kind :mouse-binding
      :context context
      :command command
      :binding-source (cond
                        (contains? command-user-overrides :bindings) :custom
                        inherited-bindings :inherited
                        :else :default)
      :fallback-context-path fallback-context-path
      :bindings (filterv :button (cond
                                   (contains? command-user-overrides :bindings) user-bindings
                                   inherited-bindings inherited-bindings
                                   :else default-bindings)))))

(defn effective-sub-command-modifier [user-overrides context command sub-cmd]
  (let [default-sub-commands (some :sub-commands (default-command-bindings context command))
        default-modifier (some #(when (= (:command %) sub-cmd) (:modifier %)) default-sub-commands)]
    (get-in user-overrides [context command :sub-commands sub-cmd] default-modifier)))

(defn- assoc-command-user-overrides [user-overrides context command command-user-overrides]
  (if (and (nil? (:bindings command-user-overrides)) (empty? (:sub-commands command-user-overrides)))
    (util/dissoc-in user-overrides [context command])
    (assoc-in user-overrides [context command] command-user-overrides)))

(defn update-command-bindings
  "Sets the binding list for a command in user-overrides. If the new bindings
  equal the registered defaults the override entry is removed, keeping
  user-overrides sparse."
  [user-overrides context command bindings]
  (let [default-bindings (mapv :binding (default-command-bindings context command))
        command-user-overrides (get-in user-overrides [context command] {})
        new-command-user-overrides (cond-> command-user-overrides
                                     (= bindings default-bindings) (dissoc :bindings)
                                     (not= bindings default-bindings) (assoc :bindings bindings))]
    (assoc-command-user-overrides user-overrides context command new-command-user-overrides)))

(defn remove-command-binding [user-overrides context command binding-index]
  (let [bindings (command-bindings-for-edit user-overrides context command)
        new-bindings (into [] (keep-indexed #(when (not= %1 binding-index) %2)) bindings)]
    (update-command-bindings user-overrides context command new-bindings)))

(defn reset-command-bindings
  "Clears the binding override for a command, restoring default binding
  behavior. Any sub-command modifier overrides for the command are preserved."
  [user-overrides context command]
  (assoc-command-user-overrides user-overrides context command (dissoc (get-in user-overrides [context command] {}) :bindings)))

(defn update-sub-command-modifier [user-overrides context command sub-cmd modifier]
  (let [default-sub-commands (some :sub-commands (default-command-bindings context command))
        default-modifier (some #(when (= (:command %) sub-cmd) (:modifier %)) default-sub-commands)
        command-user-overrides (get-in user-overrides [context command] {})
        new-command-user-overrides (if (= modifier default-modifier)
                                     (update command-user-overrides :sub-commands dissoc sub-cmd)
                                     (assoc-in command-user-overrides [:sub-commands sub-cmd] modifier))
        new-command-user-overrides (cond-> new-command-user-overrides
                                     (empty? (:sub-commands new-command-user-overrides)) (dissoc :sub-commands))]
    (assoc-command-user-overrides user-overrides context command new-command-user-overrides)))

(defn reset-sub-command-modifier [user-overrides context command sub-cmd]
  (update-sub-command-modifier user-overrides context command sub-cmd
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
        effective-modifier (get-in state [:contexts context :user-overrides command :sub-commands sub-cmd]
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
