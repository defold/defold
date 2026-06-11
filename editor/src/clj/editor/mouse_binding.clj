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
  (atom {;; contexts: {context {:bindings {command [{:binding b :action a ...}  ; regular commands
         ;;                                         ;; or
         ;;                                         {:modifier m :action a ...}]} ; modifier-only
         ;;                     :fallback-context context
         ;;                     :user-overrides {command {:bindings [binding ...]}  ; regular commands
         ;;                                              ;; or
         ;;                                      command {:modifier modifier-kw}}}} ; modifier-only
         :contexts {}}))

(defn register!
  "Registers mouse bindings for a context.

  `context` - keyword identifying the context (e.g. `:scene`)
  `context-path` - display path shown in the preferences UI
  `bindings` - sequence of command maps, each with:
    `:command` - keyword identifying the command
    `:action` - vector of display strings forming a breadcrumb, e.g. `[\"Free Look\" \"Speed Boost\"]`
    `:binding` - map with `:button` and `:modifiers` keys; mutually exclusive with `:modifier`
    `:modifier` - modifier keyword for modifier-only commands; mutually exclusive with `:binding`
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
    {context {command {:bindings [binding ...]}      ; regular commands
              modifier-cmd {:modifier modifier-kw}}} ; modifier-only commands"
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
  "Builds a prefs-UI row descriptor for a command. For regular commands returns a
  map with `:kind :mouse-binding`, `:binding-source` (:custom/:inherited/:default),
  `:fallback-context-path`, and `:bindings`. For modifier-only commands returns a
  map with `:kind :mouse-modifier`, `:modifier`, and `:default-modifier`. Both
  include template keys from the registered binding (`:context-path`, `:action`, etc.)."
  [user-overrides context command]
  (let [default-bindings-data (default-command-bindings context command)
        template (-> (first default-bindings-data)
                     (dissoc :binding :modifier)
                     (assoc :context context :command command))]
    (if-let [default-modifier (some :modifier default-bindings-data)]
      (let [effective-modifier (get-in user-overrides [context command :modifier] default-modifier)]
        (assoc template
          :kind :mouse-modifier
          :modifier effective-modifier
          :default-modifier default-modifier))
      (let [overrides (get-in user-overrides [context command])
            custom? (contains? overrides :bindings)
            defaults (mapv :binding default-bindings-data)
            fallback-ctx (when (and (not custom?) (empty? (filter some? defaults)))
                           (fallback-context context))
            fallback-data (when fallback-ctx (default-command-bindings fallback-ctx command))
            fallback-bindings (when fallback-ctx
                                (or (user-command-bindings user-overrides fallback-ctx command)
                                    (mapv :binding fallback-data)))
            inherited? (boolean (seq fallback-bindings))
            bindings (cond custom?    (:bindings overrides)
                           inherited? fallback-bindings
                           :else      defaults)]
        (assoc template
          :kind :mouse-binding
          :binding-source (cond custom? :custom
                                inherited? :inherited
                                :else :default)
          :fallback-context-path (when inherited? (some :context-path fallback-data))
          :bindings bindings)))))

(defn- assoc-command-user-overrides [user-overrides context command command-user-overrides]
  (if (empty? command-user-overrides)
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
  "Clears the binding override for a command, restoring default binding behavior."
  [user-overrides context command]
  (util/dissoc-in user-overrides [context command]))

(defn update-modifier-command
  "Sets the modifier override for a modifier-only command. If the new modifier
  equals the registered default, the override entry is removed."
  [user-overrides context command modifier]
  (let [default-modifier (some :modifier (default-command-bindings context command))]
    (if (= modifier default-modifier)
      (util/dissoc-in user-overrides [context command])
      (assoc-in user-overrides [context command :modifier] modifier))))

(defn reset-modifier-command
  "Clears the modifier override for a modifier-only command."
  [user-overrides context command]
  (util/dissoc-in user-overrides [context command]))

(defn all-bindings []
  (let [state @bindings-atom]
    (into []
          (mapcat (fn [[context {:keys [bindings]}]]
                    (map (fn [[command binding-maps]]
                           (let [template (dissoc (first binding-maps) :binding :modifier)]
                             (if (some :modifier binding-maps)
                               (assoc template
                                 :context context
                                 :command command
                                 :modifier (get-in state [:contexts context :user-overrides command :modifier]
                                                   (some :modifier binding-maps)))
                               (let [effective (effective-command-bindings* state context command binding-maps)]
                                 (assoc template
                                   :context context
                                   :command command
                                   :bindings (mapv :binding effective))))))
                         bindings)))
          (:contexts state))))

(defn bindings [context]
  (let [state @bindings-atom]
    (into []
          (mapcat (fn [[command binding-maps]]
                    (when-not (some :modifier binding-maps)
                      (effective-command-bindings* state context command binding-maps))))
          (get-in state [:contexts context :bindings]))))

(defn- command-active?* [state context command input-state]
  (let [binding-maps (get-in state [:contexts context :bindings command])]
    (or (if (some :modifier binding-maps)
          (let [default-modifier (some :modifier binding-maps)
                effective-modifier (get-in state [:contexts context :user-overrides command :modifier] default-modifier)]
            (boolean (when effective-modifier
                       (contains? (:modifiers input-state) effective-modifier))))
          (boolean
            (some (fn [mb]
                    (when-let [b (:binding mb)]
                      (i/mouse-binding-active? b input-state)))
                  (effective-command-bindings* state context command binding-maps))))
        (when-let [fallback (get-in state [:contexts context :fallback-context])]
          (command-active?* state fallback command input-state)))))

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


(defn binding-display-text [{selected-modifiers :modifiers
                             :keys [button]}]
  (if button
    (let [parts (into []
                      (comp (filter (set selected-modifiers))
                            (map modifier->label))
                      modifiers)]
      (string/join "+" (conj parts (button->label button))))
    ""))
