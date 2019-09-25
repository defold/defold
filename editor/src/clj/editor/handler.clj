(ns editor.handler
  (:require [clojure.spec.alpha :as s]
            [dynamo.graph :as g]
            [editor.analytics :as analytics]
            [editor.core :as core]
            [editor.error-reporting :as error-reporting]
            [editor.util :as util]
            [plumbing.core :refer [fnk]]))

(set! *warn-on-reflection* true)

(defn- register-handler [state handler-id command context-definition fns]
  (let [contexts (if (keyword? context-definition)
                   [context-definition]
                   context-definition)]
    (reduce
      (fn [acc context]
        (-> acc
            (assoc-in [:handlers [command context] handler-id] {:command command
                                                                :context context
                                                                :fns fns})
            (update-in [:ids handler-id :handlers] (fnil conj #{}) [command context])))
      state
      contexts)))

(defn- unregister-handler [state handler-id]
  (reduce
    (fn [acc command+context]
      (util/dissoc-in acc [:handlers command+context handler-id]))
    (util/dissoc-in state [:ids handler-id :handlers])
    (get-in state [:ids handler-id :handlers])))

(defn- register-menu [state menu-id location menu]
  (-> state
      (update-in [:menus location] (fnil conj (array-map)) [menu-id menu])
      (update-in [:ids menu-id :menus] (fnil conj #{}) location)))

(defn- unregister-menu [state menu-id]
  (reduce
    (fn [acc location]
      (util/dissoc-in acc [:menus location menu-id]))
    (util/dissoc-in state [:ids menu-id :menus])
    (get-in state [:ids menu-id :menus])))

(defn- register-dynamics [state id dynamic-handlers]
  (reduce
    (fn [acc handler]
      (let [dynamic-id (keyword (gensym "dynamic-handler"))
            {:keys [context-definition locations menu-item]
             :or {context-definition :global}} handler
            menu-item (assoc menu-item :command dynamic-id)]
        (-> acc
            (update-in [:ids id :dynamics] (fnil conj #{}) dynamic-id)
            (register-handler dynamic-id dynamic-id context-definition (:fns handler))
            (as-> x (reduce #(register-menu %1 dynamic-id %2 [menu-item]) x locations)))))
    state
    dynamic-handlers))

(defn- unregister-dynamics [state id]
  (reduce
    (fn [acc dynamic-id]
      (-> acc
          (unregister-menu dynamic-id)
          (unregister-handler dynamic-id)))
    (util/dissoc-in state [:ids id :dynamics])
    (get-in state [:ids id :dynamics])))

(defonce state-atom (atom {}))

(s/def ::menu-item
  (s/keys :opt-un [::id ::icon ::style ::label ::command ::children ::graphic-fn ::user-data ::check]))

(s/def ::menu (s/coll-of ::menu-item))
(s/def ::id keyword?)
(s/def ::icon string?)
(s/def ::style (s/coll-of string?))
(s/def ::label (s/or :string string? :separator #{:separator}))
(s/def ::command keyword?)
(s/def ::children ::menu)
(s/def ::graphic-fn fn?)
(s/def ::user-data any?)
(s/def ::check boolean?)
(s/def ::location ::id)
(s/def ::locations (s/coll-of ::location))
(s/def ::context simple-keyword?)
(s/def ::context-definition
  (s/or :one ::context
        :many (s/coll-of ::context :min-count 1 :distinct true)))
(s/def ::fns (s/map-of keyword? fn?))

(defn register-menu!
  "Register `menu` with `menu-id` on some optional `location`

  `menu-id` should be unique *per location*, meaning you can use same `menu-id`
  for different locations, and registering menu with id and location that are
  already present will override previously registered menu.

  `location` is a place to extend different menus. menu items can have `:id`
  keys, and all menus that have same `location` as item's id will be inserted
  after those items. It's also used to [[realize-menu]].

  `menu` is a coll of menu items, which are maps with these keys:
  - `:id` (optional) - item id that acts as location to insert other menus after
  - `:icon` (optional) - string path to icon image
  - `:style` (optional) - coll of strings (css classes) for item view
  - `:label` (optional) - either string (item label text) or `:separator` (will
    make item display as separator)
  - `:command` (optional) - value (usually keyword) for registered command
  - `:children` (optional) - nested menu
  - `:graphic-fn` (optional) - 0-arg function that creates a graphic node for
     this item, takes precedence over `:icon`
  - `:user-data` (optional) - any value that will be passed to associated
    command's environment as `user-data` arg
  - `:check` (optional) - boolean indicating whether this menu item should have
    a checkbox"
  ([menu-id menu]
   (register-menu! menu-id menu-id menu))
  ([menu-id location menu]
   (swap! state-atom register-menu menu-id location menu)))

(defn register-handler!
  "Register `command` executable in some `context`

  `command` is a value (usually a keyword) that identifies command

  `context-definition` is keyword (or coll of keywords) that identify when this
  command is available. There is a stack of contexts at the time when we
  determine what command is available, and first `active?` command wins

  `handler-id` should be unique *per command+context pair*. This means you can
  use same `handler-id` for different command+context combinations. It also
  means that for same command+context pair there may be multiple command
  candidates with undefined `active?` check order, so you should ensure that for
  registered command+context pair only one can be active at any given time

  `fns` is a map from predefined set of keywords to `fnk`s that accept inputs
  provided by context map. allowed keys:
  - `:run` (optional) - fnk that is invoked when command is executed
  - `:active?` (optional) - fnk predicate to determine if command is available,
    has additional `evaluation-context` argument
  - `:enabled?` (optional) - fnk predicate to determine if available command is
    enabled or disabled, had additional `evaluation-context` arg. This function
    is useful when we want some command to be present at all times so user knows
    it exists, while being enabled only when it makes sense for such command to
    be enabled
  - `:state` (optional) - fnk that returns value that is coerced to boolean and
    determines if associated menu item displayed as active or not
  - `:label` (optional) - fnk that returns label for menu item associated with
    this command, takes precedence over such item's `:label`
  - `:options` (optional) - fnk that returns list of menu items that users first
    has to choose from before running this command. When command with options is
    displayed in context menu, it gets a sub-menu with these items. When command
    is invoked via shortcut, a window pops up that requests user to select one
    of these options first. `:user-data` from these menu items will be passed as
    `:user-data` to `:run` fnk"
  [handler-id command context-definition fns]
  (swap! state-atom register-handler handler-id command context-definition fns))

(s/def ::dynamic-handler
  (s/keys :req-un [::fns ::locations ::menu-item] :opt-un [::context-definition]))

(defn register-dynamic!
  "Atomically register a coll of dynamic handlers for specified id

  Dynamic handlers don't have unique command identifiers, yet they have
  corresponding menu items that makes them reachable by user

  `dynamic-handlers` is a coll of maps with these keys:
  - `:context-definition` (optional, default `:global`) - keyword or coll of
    keywords that identify when this dynamic command is available
  - `:fns` (required, handler `fnk`s) - map from predefined set of keywords to
    functions, see [[register-handler!]]
  - `:locations` (required) - collection of menu item locations
  - `:menu-item` (required) - menu item description"
  [id dynamic-handlers]
  (swap! state-atom
         (fn [state]
           (-> state
               (unregister-dynamics id)
               (register-dynamics id dynamic-handlers)))))

(s/fdef defhandler
  :args (s/cat :command ::command
               :context ::context-definition
               :body (s/+ (s/spec (s/cat :name #{'active? 'enabled? 'label 'options 'run 'state}
                                         :args (s/coll-of simple-symbol? :kind vector?)
                                         :body (s/* any?))))))

(defmacro defhandler
  "Convenience macro for [[register-handler!]]"
  [command context-definition & body]
  (let [handler-id (keyword (str *ns*) (name command))
        fns (->> body
                 (mapcat (fn [[fname fargs & fbody]]
                           [(keyword fname) `(fnk ~fargs ~@fbody)]))
                 (apply hash-map))]
    `(register-handler! ~handler-id ~command ~context-definition ~fns)))

(defonce ^:dynamic *adapters* nil)

(defprotocol SelectionProvider
  (selection [this])
  (succeeding-selection [this])
  (alt-selection [this]))

(defrecord Context [name env selection-provider dynamics adapters])

(defn ->context
  ([name env]
   (->context name env nil))
  ([name env selection-provider]
   (->context name env selection-provider {}))
  ([name env selection-provider dynamics]
   (->context name env selection-provider dynamics {}))
  ([name env selection-provider dynamics adapters]
   (->Context name env selection-provider dynamics adapters)))

(defn available-commands
  []
  (map first (keys (:handlers @state-atom))))

(defn- get-fnk [handler fsym]
  (get-in handler [:fns fsym]))

(defonce ^:private throwing-handlers (atom #{}))

(defn enable-disabled-handlers!
  "Re-enables any handlers that were disabled because they threw an exception."
  []
  (reset! throwing-handlers #{})
  nil)

(defn- invoke-fnk [handler fsym command-context default]
  (let [env (:env command-context)
        throwing-id [(:command handler) (:context handler) fsym (:active-resource env)]]
    (if (contains? @throwing-handlers throwing-id)
      nil
      (if-let [f (get-fnk handler fsym)]
        (binding [*adapters* (:adapters command-context)]
          (try
            (f env)
            (catch Exception e
              (when (not= :run fsym)
                (swap! throwing-handlers conj throwing-id))
              (error-reporting/report-exception!
                (ex-info (format "handler '%s' in context '%s' failed at '%s' with message '%s'"
                                 (:command handler) (:context handler) fsym (.getMessage e))
                         {:handler handler
                          :command-context command-context}
                         e))
              nil)))
        default))))

(defn- get-active-handler [state command command-context evaluation-context]
  (let [ctx-name (:name command-context)
        ctx (assoc-in command-context [:env :evaluation-context] evaluation-context)]
    (some (fn [handler]
            (when (invoke-fnk handler :active? ctx true)
              handler))
          (vals (get-in state [:handlers [command ctx-name]])))))

(defn- get-active [command command-contexts user-data evaluation-context]
  (let [state @state-atom]
    (some (fn [ctx]
            (let [full-ctx (assoc-in ctx [:env :user-data] user-data)]
              (when-let [handler (get-active-handler state command full-ctx evaluation-context)]
                [handler full-ctx])))
          command-contexts)))

(defn- ctx->screen-name [ctx]
  ;; TODO distinguish between scene/form etc when workbench is the context
  (name (:name ctx)))

(defn run [[handler command-context]]
  (analytics/track-screen! (ctx->screen-name command-context))
  (invoke-fnk handler :run command-context nil))

(defn state [[handler command-context]]
  (invoke-fnk handler :state command-context nil))

(defn enabled?
  ([handler+command-context]
   (g/with-auto-evaluation-context evaluation-context
     (enabled? handler+command-context evaluation-context)))
  ([[handler command-context] evaluation-context]
   (let [ctx (assoc-in command-context [:env :evaluation-context] evaluation-context)]
     (boolean (invoke-fnk handler :enabled? ctx true)))))

(defn label [[handler command-context]]
  (invoke-fnk handler :label command-context nil))

(defn options [[handler command-context]]
  (invoke-fnk handler :options command-context nil))

(defn- eval-dynamics [context]
  (cond-> context
    (contains? context :dynamics)
    (update :env merge (into {} (map (fn [[k [node v]]] [k (g/node-value (get-in context [:env node]) v)]) (:dynamics context))))))

(defn active
  ([command command-contexts user-data]
   (g/with-auto-evaluation-context evaluation-context
     (active command command-contexts user-data evaluation-context)))
  ([command command-contexts user-data evaluation-context]
   (get-active command command-contexts user-data evaluation-context)))

(defn- context-selections [context]
  (if-let [s (get-in context [:env :selection])]
    [s]
    (if-let [sp (:selection-provider context)]
      (let [s (selection sp)
            alt-s (alt-selection sp)]
        (if (and (seq alt-s) (not= s alt-s))
          [s alt-s]
          [s]))
      [nil])))

(defn- eval-selection-context [context]
  (let [selections (context-selections context)]
    (mapv (fn [selection]
            (update context :env assoc :selection selection :selection-context (:name context) :selection-provider (:selection-provider context)))
          selections)))

(defn eval-contexts [contexts all-selections?]
  (let [contexts (mapv eval-dynamics contexts)]
    (loop [selection-contexts (mapcat eval-selection-context contexts)
           result []]
      (if-let [ctx (and (or all-selections?
                            (= (:name (first selection-contexts)) (:name (first contexts))))
                        (first selection-contexts))]
        (let [result (if-let [selection (get-in ctx [:env :selection])]
                       (let [adapters (:adapters ctx)
                             name (:name ctx)
                             selection-provider (:selection-provider ctx)]
                         (into result (map (fn [ctx] (-> ctx
                                                       (update :env assoc :selection selection :selection-context name :selection-provider selection-provider)
                                                       (assoc :adapters adapters)))
                                           selection-contexts)))
                       (conj result ctx))]
          (recur (rest selection-contexts) result))
        result))))

(defn- items-at-location [menus location]
  (into [] cat (vals (get menus location))))

(defn- do-realize-menu [items menus]
  (into []
        (comp
          (map
            (fn [item]
              (cond-> item
                      (:children item)
                      (update :children do-realize-menu menus))))
          (mapcat
            (fn [item]
              (cond-> [item]
                      (and (contains? item :id) (contains? menus (:id item)))
                      (into (do-realize-menu (items-at-location menus (:id item)) menus))))))
        items))

(defn realize-menu [location]
  (let [menus (:menus @state-atom)]
    (do-realize-menu (items-at-location menus location) menus)))

(defn adapt [selection t]
  (if (empty? selection)
    selection
    (let [selection (if (g/isa-node-type? t)
                      (adapt selection Long)
                      selection)
          adapters *adapters*
          v (first selection)
          f (cond
              (isa? (type v) t) identity
              ;; this is somewhat of a hack, copied from clojure internal source
              ;; there does not seem to be a way to test if a type is a protocol
              (and (:on-interface t) (instance? (:on-interface t) v)) identity
              ;; test for node types specifically by checking for longs
              ;; we can't use g/NodeID because that is actually a wrapped ValueTypeRef
              (and (g/isa-node-type? t) (= (type v) Long)) (fn [v] (when (g/node-instance? t v) v))
              (satisfies? core/Adaptable v) (fn [v] (core/adapt v t))
              true (get adapters t (constantly nil)))]
      (mapv f selection))))

(defn adapt-every
  ([selection t]
   (adapt-every selection t some?))
  ([selection t pred]
   (if (empty? selection)
     nil
     (let [s' (adapt selection t)]
       (if (every? pred s')
         s'
         nil)))))

(defn adapt-single [selection t]
  (when (and (nil? (next selection)) (first selection))
    (first (adapt selection t))))

(defn selection->node-ids
  ([selection]
   (adapt-every selection Long))
  ([selection pred]
   (adapt-every selection Long pred)))

(defn selection->node-id [selection]
  (adapt-single selection Long))
