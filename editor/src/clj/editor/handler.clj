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

(ns editor.handler
  (:require [clojure.spec.alpha :as s]
            [dynamo.graph :as g]
            [editor.analytics :as analytics]
            [editor.core :as core]
            [editor.error-reporting :as error-reporting]
            [editor.localization :as localization]
            [editor.util :as util]
            [plumbing.core :refer [fnk]]
            [util.coll :as coll :refer [pair]]
            [util.defonce :as defonce]
            [util.eduction :as e])
  (:import [clojure.lang RT]))

(set! *warn-on-reflection* true)

(defn add-handlers [command+context->registration->handler handlers registration]
  (reduce
    (fn [acc [command+context handler]]
      (update acc command+context assoc registration handler))
    command+context->registration->handler
    (e/mapcat
      (fn [{:keys [contexts command] :as handler}]
        (e/map #(pair (pair command %) handler) contexts))
      handlers)))

(defn- remove-handlers [command+context->registration->handler command+contexts registration]
  (reduce
    (fn [acc command+context]
      (util/dissoc-in acc [command+context registration]))
    command+context->registration->handler
    command+contexts))

(defn- add-menus [location->registration->menu menus registration]
  (reduce-kv
    (fn [acc location items]
      (assoc-in acc [location registration] items))
    location->registration->menu
    menus))

(defn- remove-menus [location->registration->menu locations registration]
  (reduce
    (fn [acc location]
      (util/dissoc-in acc [location registration]))
    location->registration->menu
    locations))

(defn- unregister [{:keys [registrations] :as state} registration]
  (if-let [{:keys [command+contexts locations]} (registrations registration)]
    (-> state
        (assoc :registrations (dissoc registrations registration))
        (update :handlers remove-handlers command+contexts registration)
        (update :menus remove-menus locations registration))
    state))

(def ^:private synthetic-command-str "synthetic-command")

(defn synthetic-command? [x]
  (= (namespace x) synthetic-command-str))

(defn private-command? [x]
  (case (namespace x)
    "private" true
    false))

(defn- register [state registration menus handlers]
  (let [handlers (mapv (fn [h]
                         (if (contains? h :command)
                           h
                           (assoc h :command (keyword synthetic-command-str (str synthetic-command-str (RT/nextID))))))
                       handlers)
        menus (reduce
                (fn [acc [location menu-item]]
                  (update acc location (fnil conj []) menu-item))
                menus
                (e/mapcat
                  (fn [{:keys [command locations]}]
                    (let [menu-item {:command command}]
                      (e/map #(pair % menu-item) locations)))
                  handlers))]
    (-> state
        (unregister registration)
        (update :menus add-menus menus registration)
        (update :handlers add-handlers handlers registration)
        (update :registrations assoc registration {:locations (vec (keys menus))
                                                   :command+contexts (vec (e/mapcat
                                                                            (fn [{:keys [command contexts]}]
                                                                              (e/map #(pair command %) contexts))
                                                                            handlers))}))))
(def empty-state
  {;; handlers: {command+context {registration handler}
   :handlers {}
   ;; registrations: {registration {:command+contexts [command+context]
   ;;                               :locations [location]}}
   :registrations {}
   ;; menus: {location {registration [menu-item]}}
   :menus {}})

(defonce state-atom
  (atom empty-state))

;; menu

(s/def :editor.menu/menu (s/coll-of :editor.menu/menu-item))
(s/def :editor.menu/location keyword?)
(s/def :editor.menu/menu-item
  (s/keys :opt-un [:editor.menu/command
                   :editor.menu/label
                   :editor.menu/children
                   :editor.menu/id
                   :editor.menu/icon
                   :editor.menu/graphic-fn
                   :editor.menu/style
                   :editor.menu/user-data
                   :editor.menu/check
                   :editor.menu/on-submenu-open]))
(s/def :editor.menu/command (s/or :synthetic-command synthetic-command? :named-command keyword?))
(s/def :editor.menu/label (s/or :string string? :message localization/message-pattern? :separator #{:separator}))
(s/def :editor.menu/children :editor.menu/menu)
(s/def :editor.menu/id :editor.menu/location)
(s/def :editor.menu/icon string?)
(s/def :editor.menu/graphic-fn fn?)
(s/def :editor.menu/style (s/coll-of string?))
(s/def :editor.menu/user-data any?)
(s/def :editor.menu/check boolean?)
(s/def :editor.menu/on-submenu-open fn?)

;; handler

(s/def ::handler
  (s/keys :req-un [::contexts]
          :opt-un [::command
                   ::locations
                   ::run
                   ::active?
                   ::enabled?
                   ::state
                   ::label
                   ::options]))
(s/def ::contexts (s/coll-of keyword? :min-count 1 :distinct true))
(s/def ::command keyword?)
(s/def ::locations (s/coll-of :editor.menu/location))
(s/def ::run fn?)
(s/def ::active? fn?)
(s/def ::enabled? fn?)
(s/def ::state fn?)
(s/def ::label (s/or :fn fn? :string string? :message localization/message-pattern?))
(s/def ::options fn?)

;; register args

(s/def ::menus (s/map-of :editor.menu/location :editor.menu/menu))
(s/def ::handlers (s/coll-of ::handler))

(defn register!
  "Atomically register menus and/or handlers

  Args:
    registration    the registration identifier, e.g. a keyword

  Kv-args (all optional):
    :menus       a map from location identifier (a keyword) to a collection of
                 menu items, where each menu item is a map with the following
                 optional keys:
                   :command            a keyword command identifier
                   :label              either a string (item label text),
                                       localization MessagePattern, or
                                       :separator (will display the menu item
                                       as a separator)
                   :children           nested collection of menu items
                   :id                 menu item location identifier - other
                                       menus will be inserted after it if their
                                       location is the same as the id
                   :icon               string path to icon image
                   :graphic-fn         0-arg function that creates a graphic
                                       Node for this item, takes precedence
                                       over :icon
                   :style              coll of strings (css classes) for the
                                       item view
                   :user-data          any value that will be passed to the
                                       command environment as a user-data arg
                   :check              boolean indicating whether this menu
                                       item should have a checkbox
                   :on-submenu-open    0-arg function that is invoked before the
                                       menu dropdown is presented
    :handlers    a collection of command handlers, where each handler is a map
                 with the following keys:
                   :contexts     required, non-empty collection of contexts
                                 (keywords like :global, :asset-browser,
                                 :code-view etc) that identify where this
                                 command is available
                   :command      optional keyword identifier that may be used by
                                 the editor for invoking the handler in the
                                 right context; if not specified, a synthetic
                                 command identifier will be generated — make
                                 sure to not preserve it anywhere, since it will
                                 be different in different editor runs; use
                                 `handler/synthetic-command?` predicate to test
                                 if the command is synthetic or not
                   :locations    collection of menu item location identifiers
                                 that define where this command is presented in
                                 the menu views; required if command is not
                                 defined — otherwise there will be no way to
                                 invoke the handler
                   :run          optional fnk, invoked when the command handler
                                 is executed
                   :active?      optional fnk predicate, determines if handler
                                 is available (i.e. visible in the menu);
                                 receives additional :evaluation-context arg
                   :enabled?     optional fnk predicate, determines if active
                                 handler is enabled (i.e. can be invoked);
                                 receives additional :evaluation-context arg.
                                 This function is useful when we want some
                                 command to be present at all times so the user
                                 knows it exists while enabling it only when it
                                 makes sense
                   :state        optional fnk that returns value that is then
                                 coerced to boolean and gets presented as a
                                 checked or unchecked menu item
                   :label        optional, either a string, localization
                                 MessagePattern, or an fnk that returns
                                 for the menu item associated with this command;
                                 takes precedence over label defined on the menu
                                 item
                   :options      optional fnk that returns a list of menu items
                                 that the user has to choose from instead of
                                 running this command; presented as a drop-down
                                 submenu. When invoked via shortcut, a window
                                 pops up that asks the user to select one of the
                                 options"
  [registration & {:keys [menus handlers]}]
  {:pre [(or (nil? menus) (s/assert ::menus menus))
         (or (nil? handlers) (s/assert ::handlers handlers))]}
  (swap! state-atom register registration menus handlers)
  nil)

(defn unregister!
  "Atomically unregister previously registered menus and/or handlers

  Args:
    registration    the registration identifier, e.g. a keyword"
  [registration]
  (swap! state-atom unregister registration)
  nil)

(s/def ::defhandler-body (s/+ (s/alt :kv (s/cat :key keyword? :val any?)
                                     :fn (s/spec (s/cat :name simple-symbol?
                                                        :args (s/coll-of simple-symbol? :kind vector?)
                                                        :body (s/* any?))))))
(s/fdef defhandler
  :args (s/cat :command keyword?
               :context keyword?
               :body ::defhandler-body))

(defmacro defhandler [command context & body]
  (let [registration-id (keyword (str *ns*) (str (name command) ":" (name context)))
        handler (into {:command command
                       :contexts [context]}
                      (map (fn [[alt m]]
                             (case alt
                               :kv [(:key m) (:val m)]
                               :fn [(keyword (:name m)) `(fnk ~(:args m) ~@(:body m))])))
                      (s/conform ::defhandler-body body))]
    `(register! ~registration-id :handlers [~handler])))

(defn register-menu!
  ([location menu]
   (register-menu! location location menu))
  ([registration location menu]
   (register! registration :menus {location menu})))

(defn register-handler!
  [registration command context handler]
  (register! registration :handlers [(assoc handler :command command :contexts [context])]))

(defonce ^:dynamic *adapters* nil)

(defonce/protocol SelectionProvider
  (selection [this])
  (succeeding-selection [this])
  (alt-selection [this]))

(defonce/record Context [name env selection-provider dynamics adapters])

(defn ->context
  ([name env]
   (->context name env nil))
  ([name env selection-provider]
   (->context name env selection-provider {}))
  ([name env selection-provider dynamics]
   (->context name env selection-provider dynamics {}))
  ([name env selection-provider dynamics adapters]
   (->Context name env selection-provider dynamics adapters)))

(defn public-commands
  "Returns a set of user-facing commands that may be e.g. assigned shortcuts"
  ([]
   (public-commands @state-atom))
  ([handler-state]
   (->> handler-state
        :handlers
        keys
        (e/map first)
        (e/remove synthetic-command?)
        (e/remove private-command?)
        (into #{}))))

(defonce ^:private throwing-handlers (atom {}))

(defn enable-disabled-handlers!
  "Re-enables any handlers that were disabled because they threw an exception."
  []
  (reset! throwing-handlers {})
  nil)

(defn- invoke-fnk [handler fsym command-context default]
  (let [fnk (get handler fsym)]
    (if (nil? fnk)
      default
      (let [env (:env command-context)
            throwing-id [(:command handler) (:name command-context) fsym (:active-resource env)]
            throwing-fnk (get @throwing-handlers throwing-id)]
        (when-not (identical? throwing-fnk fnk)
          (when (some? throwing-fnk)
            ;; Looks like the handler was redefined. Clear out the throwing-id,
            ;; then proceed to give it a go.
            (swap! throwing-handlers dissoc throwing-id))
          (binding [*adapters* (:adapters command-context)]
            (try
              (fnk env)
              (catch Exception e
                (when (not= :run fsym)
                  (swap! throwing-handlers assoc throwing-id fnk))
                (error-reporting/report-exception!
                  (ex-info (format "handler '%s' in context '%s' failed at '%s' with message '%s'"
                                   (:command handler) (:name command-context) fsym (.getMessage e))
                           {:handler handler
                            :command-context (update command-context :env dissoc :evaluation-context)}
                           e))
                nil))))))))

(defn- get-active-handler [state command command-context evaluation-context]
  (let [ctx-name (:name command-context)
        ctx (assoc-in command-context [:env :evaluation-context] evaluation-context)]
    (some (fn [handler]
            (when (invoke-fnk handler :active? ctx true)
              handler))
          (vals (get-in state [:handlers [command ctx-name]])))))

(defn- ctx->screen-name [ctx]
  ;; TODO distinguish between scene/form etc when workbench is the context
  (name (:name ctx)))

(defn- invoke-fnk-ec [handler fsym command-context default evaluation-context]
  (if (= ::auto-evaluation-context evaluation-context)
    (g/with-auto-evaluation-context evaluation-context
      (let [ctx (assoc-in command-context [:env :evaluation-context] evaluation-context)]
        (invoke-fnk handler fsym ctx default)))
    (let [ctx (assoc-in command-context [:env :evaluation-context] evaluation-context)]
      (invoke-fnk handler fsym ctx default))))

(defn run [[handler command-context]]
  (analytics/track-screen! (ctx->screen-name command-context))
  (invoke-fnk handler :run command-context nil))

(defn state
  ([[handler command-context]]
   (invoke-fnk-ec handler :state command-context nil ::auto-evaluation-context))
  ([[handler command-context] evaluation-context]
   (invoke-fnk-ec handler :state command-context nil evaluation-context)))

(defn enabled?
  ([[handler command-context]]
   (boolean (invoke-fnk-ec handler :enabled? command-context true ::auto-evaluation-context)))
  ([[handler command-context] evaluation-context]
   (boolean (invoke-fnk-ec handler :enabled? command-context true evaluation-context))))

(defn label
  ([handler+command-context]
   (label handler+command-context ::auto-evaluation-context))
  ([[handler command-context] evaluation-context]
   (let [label (:label handler)]
     (if (or (string? label)
             (localization/message-pattern? label))
       label
       (invoke-fnk-ec handler :label command-context nil evaluation-context)))))

(defn options
  ([[handler command-context]]
   (invoke-fnk-ec handler :options command-context nil ::auto-evaluation-context))
  ([[handler command-context] evaluation-context]
   (invoke-fnk-ec handler :options command-context nil evaluation-context)))

(defn- flatten-menu-item-tree [item]
  (->> item
       :children
       (e/mapcat (fn [child-item]
                   (-> child-item
                       (cond-> (not= :separator (:label child-item)) (update :label #(localization/join " → " [(:label item) %])))
                       (flatten-menu-item-tree))))
       (e/cons item)))

(defn option-items
  "Produce a flat list of options from menu items returned from options handler

  Returns either a non-empty vector or nil"
  [[{:keys [command]} :as handler+command-context]]
  (when-let [opts (options handler+command-context)] ; Safe to not supply evaluation-context - we're executing a command.
    (when-let [flat-opts (->> opts
                              (e/mapcat flatten-menu-item-tree)
                              (e/remove #(= :separator (:label %)))
                              (e/filter #(= command (:command %)))
                              vec
                              coll/not-empty)]
      (with-meta flat-opts (meta opts)))))

(defn- eval-dynamics [context evaluation-context]
  (cond-> context
          (contains? context :dynamics)
          (update :env
                  (fn [env]
                    (into env
                          (map (fn [[dynamic-key [node-key label]]]
                                 (let [node-id (env node-key)
                                       value (g/node-value node-id label evaluation-context)]
                                   (pair dynamic-key value))))
                          (:dynamics context))))))

(defn active
  ([command command-contexts]
   (g/with-auto-evaluation-context evaluation-context
     (active command command-contexts nil evaluation-context)))
  ([command command-contexts user-data]
   (g/with-auto-evaluation-context evaluation-context
     (active command command-contexts user-data evaluation-context)))
  ([command command-contexts user-data evaluation-context]
   (let [state @state-atom]
     (some (fn [ctx]
             (let [full-ctx (update ctx :env assoc :user-data user-data :_context ctx)]
               (when-let [handler (get-active-handler state command full-ctx evaluation-context)]
                 [handler full-ctx])))
           command-contexts))))

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

(defn- eval-selection-context [{:keys [name selection-provider] :as context}]
  (let [selections (context-selections context)]
    (mapv (fn [selection]
            (update context :env assoc
                    :selection selection
                    :selection-context name
                    :selection-provider selection-provider))
          selections)))

(defn eval-contexts [contexts all-selections?]
  (let [contexts (g/with-auto-evaluation-context evaluation-context
                   (mapv #(eval-dynamics % evaluation-context)
                         contexts))]
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
    (let [selection (if (g/node-type? t)
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
              (and (g/node-type? t) (= (type v) Long)) (fn [v] (when (g/node-instance? t v) v))
              (satisfies? core/Adaptable v) (fn [v] (core/adapt v t))
              true (get adapters t (constantly nil)))]
      (mapv f selection))))

(defn adapt-every
  ([selection t]
   (adapt-every selection t nil))
  ([selection t pred]
   (if (empty? selection)
     nil
     (let [s' (adapt selection t)]
       (if (every? (if pred (every-pred some? pred) some?) s')
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
