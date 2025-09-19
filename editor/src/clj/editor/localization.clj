;; Copyright 2020-2025 The Defold Foundation
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

(ns editor.localization
  (:require [camel-snake-kebab :as camel]
            [clojure.java.io :as io]
            [clojure.string :as string]
            [editor.code.lang.java-properties :as java-properties]
            [editor.connection-properties :as connection-properties]
            [editor.error-reporting :as error-reporting]
            [editor.fs :as fs]
            [editor.prefs :as prefs]
            [editor.system :as system]
            [editor.ui :as ui]
            [internal.java :as java]
            [internal.util :as util]
            [util.coll :as coll]
            [util.defonce :as defonce]
            [util.eduction :as e])
  (:import [clojure.lang AFn Agent IFn IRef]
           [com.ibm.icu.text DateFormat ListFormatter ListFormatter$Type ListFormatter$Width MessageFormat]
           [com.ibm.icu.util ULocale]
           [java.io StringReader]
           [java.nio.file StandardWatchEventKinds WatchEvent$Kind]
           [java.util Collection WeakHashMap]
           [javafx.application Platform]
           [javafx.beans.value WritableValue]
           [javafx.scene.control Labeled Tab]
           [javafx.scene.text Text]
           [org.apache.commons.io FilenameUtils]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

;; Implementation note. Localization system uses an agent with different
;; executors used for different tasks:
;; - reload of localization is performed using a send-off (potentially blocking)
;;   executor because it might require reading files from the file system
;; - refresh of listeners is performed using javafx executor because the
;;   listeners are expected to be JavaFX nodes.
;; Localization data shape:
;; (Localization.
;;   ;; prefs, used for reading/storing selected locale
;;   prefs
;;   (agent
;;     (LocalizationState.
;;       {;; The following fields may be modified using public API:
;;        ;; - current locale selection, e.g. "en" or "en_US"; settable
;;        :locale "en"
;;        ;; - a map of localization bundles; settable
;;        :bundles {bundle-key {resource-path-str java.io.Reader-fn}}
;;        ;; - a WeakHashMap (objects are weakly referenced: no need to clean up listeners)
;;        :listeners {Localizable MessagePattern}
;;
;;        ;; The following fields are updated on locale and bundle change:
;;        ;; - message map for the current locale
;;        :messages {message-key fn:arg->string}
;;        ;; - for UI: sorted list of supported locales, sourced from registered
;;        ;;   bundles
;;        :available-locales ["en" "sv"]
;;        ;; - locale-specific formatter of lists like `a, b, and c`
;;        :list-and com.ibm.icu.text.ListFormatter
;;        ;; - locale-specific formatter of lists like `a, b, or c`
;;        :list-or com.ibm.icu.text.ListFormatter
;;        ;; - locale-specific date formatter (e.g. 2025-09-17 or 9/17/25)
;;        :date com.ibm.icu.text.DateFormat}))))

(defonce/protocol Localizable
  (apply-localization [obj str] "Apply a localization string to object"))

(extend-protocol Localizable
  Labeled
  (apply-localization [label str]
    (.setText label str))
  Text
  (apply-localization [text str]
    (.setText text str))
  Tab
  (apply-localization [tab str]
    (.setText tab str))
  WritableValue
  (apply-localization [tab str]
    (.setValue tab str))
  Object
  (apply-localization [unknown _]
    (throw (IllegalArgumentException. (format "%s does not implement Localizable protocol" unknown))))
  nil
  (apply-localization [_ _]
    (throw (IllegalArgumentException. "localized object must not be nil"))))

(defonce/interface MessagePattern
  (localize [state]))

(defn- message-pattern? [x]
  (instance? MessagePattern x))

(defn- impl-localize [state v]
  (if (message-pattern? v)
    (.localize ^MessagePattern v state)
    (str v)))

(defonce/record LocalizationState [locale bundles listeners messages available-locales list-and list-or date]
  IFn
  (invoke [this v] (impl-localize this v))
  (applyTo [this args] (AFn/applyToHelper this args)))

(defn- localization-state? [x]
  (instance? LocalizationState x))

(defn- refresh-messages! [^LocalizationState state]
  (let [u-locale (ULocale/createCanonical ^String (.-locale state))
        u-locale-chain (vec
                         (e/distinct
                           (e/conj
                             (->> u-locale
                                  (iterate ^[] ULocale/.getFallback)
                                  (e/take-while #(not (coll/empty? (str %)))))
                             (ULocale/createCanonical "en"))))
        u-locale->reader-fns (->> state
                                  .-bundles
                                  (e/mapcat val)
                                  (util/group-into
                                    {} []
                                    #(ULocale/createCanonical (FilenameUtils/getBaseName (key %)))
                                    val))
        ;; messages
        messages (->> u-locale-chain
                      (e/mapcat (fn [u-locale]
                                  (when-let [reader-fns (u-locale->reader-fns u-locale)]
                                    (->> reader-fns
                                         (e/mapcat #(java-properties/parse (%)))
                                         (e/map #(vector u-locale (key %) (val %)))))))
                      (reduce
                        (fn [acc [^ULocale u-locale k ^String v]]
                          (if (acc k)
                            acc
                            (assoc! acc k (try
                                            (let [mf (MessageFormat. v u-locale)]
                                              (fn format-message [arg]
                                                (try
                                                  (.format mf arg)
                                                  (catch Exception e
                                                    (if (system/defold-dev?)
                                                      (str "!" k "! " (or (ex-message e) (.getSimpleName (class e))))
                                                      v)))))
                                            (catch Exception e
                                              (fn format-error-message [_]
                                                (if (system/defold-dev?)
                                                  (str "!" k "! `" v "`: " (or (ex-message e) (.getSimpleName (class e))))
                                                  v)))))))
                        (transient {}))
                      persistent!)]
    (assoc state
      :messages messages
      :date (DateFormat/getDateInstance DateFormat/SHORT u-locale)
      :list-and (ListFormatter/getInstance u-locale ListFormatter$Type/AND ListFormatter$Width/WIDE)
      :list-or (ListFormatter/getInstance u-locale ListFormatter$Type/OR ListFormatter$Width/WIDE)
      :available-locales (->> u-locale->reader-fns
                              (e/map #(-> % key str))
                              (vec)
                              (sort)
                              (vec)))))

(defonce/type Localization [prefs ^Agent agent]
  IRef
  (deref [_] @agent)
  (setValidator [_ vf] (.setValidator agent vf))
  (getValidator [_] (.getValidator agent))
  (getWatches [_] (.getWatches agent))
  (addWatch [_ key callback] (.addWatch agent key callback))
  (removeWatch [_ key] (.removeWatch agent key))
  IFn
  (invoke [_ v] (impl-localize @agent v))
  (applyTo [this args] (AFn/applyToHelper this args)))

(defn- localization? [x]
  (instance? Localization x))

(defn- impl-set-bundle! [state key bundle]
  (-> state
      (update :bundles assoc key bundle)
      refresh-messages!))

(defn- impl-set-locale! [state prefs locale]
  (prefs/set! prefs [:window :locale] locale)
  (-> state
      (assoc :locale locale)
      refresh-messages!))

(defn- impl-localize! [^LocalizationState state object message-pattern]
  (assert (Platform/isFxApplicationThread))
  (.put ^WeakHashMap (.-listeners state) object message-pattern)
  state)

(defn- impl-unlocalize! [^LocalizationState state object]
  (assert (Platform/isFxApplicationThread))
  (.remove ^WeakHashMap (.-listeners state) object)
  state)

(defn- refresh-listeners! [^LocalizationState state]
  (assert (Platform/isFxApplicationThread))
  (.forEach ^WeakHashMap (.-listeners state) #(apply-localization %1 (.localize ^MessagePattern %2 state)))
  state)

(defn available-locales
  "Get a list of available locales that have translations

  Args:
    state    localization state (dereferenced localization)"
  [^LocalizationState state]
  {:pre [(localization-state? state)]}
  (.-available-locales state))

(defn current-locale
  "Get current locale (a string)

  Args:
    state    localization state (dereferenced localization)"
  [^LocalizationState state]
  {:pre [(localization-state? state)]}
  (.-locale state))

(defn defines-message-key?
  "Checks if the message bundle contains a particular message key

  Args:
    state          localization state (dereferenced localization)
    message-key    message key for [[message]] fn"
  [^LocalizationState state message-key]
  {:pre [(localization-state? state) (string? message-key)]}
  (contains? (.-messages state) message-key))

(defn- send-without-thread-binding-reset [executor ^Agent agent f & args]
  ;; By default, send-via/send/send-off wrap sent functions using
  ;; binding-conveyor-fn which resets the thread bindings on invocation. This
  ;; means that previous binding stack will be discarded on that thread. The
  ;; expectation seems to be that agent executors don't care about the binding
  ;; stack. This may be okay for Clojure's default agent executors, but it's not
  ;; good for the JavaFX application thread where we use bindings.
  (.dispatch agent f args executor))

(defn set-bundle!
  "Asynchronously update localization bundle

  Args:
    localization    the localization instance created with [[make]]
    bundle-key      bundle identifier, e.g. a keyword
    bundle          the bundle map, a map from localization file paths to 0-arg
                    functions that produce a java.io.Reader for java properties
                    file"
  [^Localization localization bundle-key bundle]
  {:pre [(localization? localization)]}
  (send-off (.-agent localization) impl-set-bundle! bundle-key bundle)
  (send-without-thread-binding-reset ui/javafx-executor (.-agent localization) refresh-listeners!))

(defn set-locale!
  "Asynchronously set the active locale and notify listeners

  Args:
    localization    the localization instance created with [[make]]
    locale          a locale identifier string (e.g. \"en\", \"en_US\")

  Persists the selected locale in user prefs and refreshes all
  registered listeners on the JavaFX thread."
  [^Localization localization locale]
  {:pre [(localization? localization) (string? locale)]}
  (send-off (.-agent localization) impl-set-locale! (.-prefs localization) locale)
  (send-without-thread-binding-reset ui/javafx-executor (.-agent localization) refresh-listeners!))

(defn localize!
  "Localize an object that implements Localizable protocol

  Immediately applies localization to the object with the current message value;
  and does it again on JavaFX thread whenever the locale or localization bundle
  change. The object will be stored in a weak reference: there is no need to
  unregister the localization listener.

  Returns the passed object

  Args:
    object          target object, must satisfy Localizable, will be stored in
                    a weak reference
    localization    the localization instance created with [[make]]
    pattern         a MessagePattern instance, created with, e.g., [[message]]"
  ([object ^Localization localization pattern]
   {:pre [(some? object) (localization? localization) (message-pattern? pattern)]}
   (apply-localization object (localization pattern))
   (send-without-thread-binding-reset ui/javafx-executor (.-agent localization) impl-localize! object pattern)
   object))

(defn unlocalize!
  "Stop localizing an object that was previously [[localize!]]-d

  Generally, un-localizing is not necessary for objects that are no longer used
  since they are stored in a weak reference in localization; use this function
  only when you intend to keep the reference to the object, but no longer want
  it be localized

  Returns the passed object

  Args:
    object          target object that was previously [[localize!]]-d
    localization    the localization instance created with [[make]]"
  [object ^Localization localization]
  {:pre [(some? object) (localization? localization)]}
  (send-without-thread-binding-reset ui/javafx-executor (.-agent localization) impl-unlocalize! object)
  object)

(defn make
  "Create a localization instance (agent) with initial bundle

  Args:
    prefs                  preferences handle used to persist locale selection
    initial-bundle-key     identifier for the initial bundle (e.g. a keyword)
    initial-bundle         initial bundle map, a map from localization file
                           paths to 0-arg functions that produce
                           a java.io.Reader for java properties file

  Returns a localization system agent"
  [prefs initial-bundle-key initial-bundle]
  (->Localization
    prefs
    (agent
      (impl-set-bundle!
        (map->LocalizationState
          {:locale (prefs/get prefs [:window :locale])
           :bundles {}
           :listeners (WeakHashMap.)})
        initial-bundle-key initial-bundle)
      :error-handler (fn report-localization-error [_ exception]
                       (error-reporting/report-exception! exception)))))

(defn make-editor
  "Create localization configured for use in the editor"
  [prefs]
  (letfn [(generated-localization []
            (->> {"prefs.extensions.build-server.prompt" (connection-properties/defold-build-server-url)}
                 (e/map #(str (key %) "=" (val %)))
                 (coll/join-to-string "\n")))
          (get-bundle [resource-dir]
            (let [generated (generated-localization)]
              (->> resource-dir
                   (fs/class-path-walker java/class-loader)
                   (e/filter #(.endsWith (str %) ".editor_localization"))
                   (e/map
                     (coll/pair-fn
                       str
                       (fn [path]
                         ;; We can read Path only during iteration (because we open
                         ;; the zip file), but the actual reading is done later,
                         ;; during reload. Converting path to a URL allows reading
                         ;; from the zip file later.
                         (let [url (.toURL (.toUri (fs/path path)))]
                           #(io/reader url)))))
                   (into {"generated/en.editor_localization" #(StringReader. generated)}))))]
    (let [resource-dir "localization"
          localization (make prefs ::editor (get-bundle resource-dir))]
      (when (system/defold-dev?)
        (let [url (io/resource resource-dir)]
          (when (.startsWith (str url) "file:")
            (let [resource-path (fs/path url)
                  watch-service (.newWatchService (.getFileSystem resource-path))]
              (.register resource-path watch-service (into-array WatchEvent$Kind [StandardWatchEventKinds/ENTRY_CREATE
                                                                                  StandardWatchEventKinds/ENTRY_DELETE
                                                                                  StandardWatchEventKinds/ENTRY_MODIFY
                                                                                  StandardWatchEventKinds/OVERFLOW]))
              (future
                (error-reporting/catch-all!
                  (while true
                    (let [watch-key (.take watch-service)]
                      (when (pos? (count (.pollEvents watch-key)))
                        (set-bundle! localization ::editor (get-bundle resource-dir)))
                      (.reset watch-key)))))))))
      localization)))

(defn- impl-simple-message [k m ^LocalizationState state]
  (if-let [format-fn (get (.-messages state) k)]
    (format-fn m)
    (if (system/defold-dev?)
      (str "!" k "!")
      (camel/->TitleCase (peek (string/split k #"\."))))))

(defonce/record SimpleMessage [k m]
  MessagePattern
  (localize [_ state] (impl-simple-message k m state)))

(defonce/record MessageWithNestedPatterns [k m ks]
  MessagePattern
  (localize [_ state]
    (impl-simple-message
      k
      (persistent!
        (reduce
          #(assoc! %1 %2 (.localize ^MessagePattern (m %2) state))
          (transient m)
          ks))
      state)))

(defn message
  "Create a message pattern

  To actually localize, invoke localization (or its state) with pattern

  Args:
    k    localization key, a dot-separated string, e.g. \"some-dialog.title\"
    m    optional map from string keys (variable names available for use in k's
         localization message) to either strings, numbers, or message patterns"
  ([k]
   (message k {}))
  ([k m]
   (if-let [localizable-keys (when (pos? (count m))
                               (coll/not-empty
                                 (persistent!
                                   (reduce-kv
                                     #(cond-> %1 (message-pattern? %3) (conj! %2))
                                     (transient [])
                                     m))))]
     (->MessageWithNestedPatterns k m localizable-keys)
     (->SimpleMessage k m))))

(defn- impl-simple-list [list-k items state]
  (.format ^ListFormatter (list-k state) ^Collection items))

(defonce/record SimpleList [list-k items]
  MessagePattern
  (localize [_ state] (impl-simple-list list-k items state)))

(defonce/record ListWithNestedPatterns [list-k items localizable-indices]
  MessagePattern
  (localize [_ state]
    (impl-simple-list
      list-k
      (persistent!
        (reduce
          #(assoc! %1 %2 (.localize ^MessagePattern (items %2) state))
          (transient items)
          localizable-indices))
      state)))

(defn- impl-list [k items]
  (if-let [localizable-indices (when (pos? (count items))
                                 (coll/not-empty
                                   (into []
                                         (keep-indexed
                                           (fn [i v]
                                             (when (message-pattern? v) i)))
                                         items)))]
    (->ListWithNestedPatterns k items localizable-indices)
    (->SimpleList k items)))

(defn and-list
  "Create a list message pattern using \"and\" conjunction (e.g., a, b, and c)

  To actually localize, invoke localization (or its state) with pattern

  Args:
    items    vector of items, either strings, numbers, or other localizable
             messages"
  [items]
  {:pre [(vector? items)]}
  (impl-list :list-and items))

(defn or-list
  "Create a list message pattern using \"or\" conjunction (e.g., a, b, or c)

  To actually localize, invoke localization (or its state) with pattern

  Args:
    items    vector of items, either strings, numbers, or other localizable
             messages"
  [items]
  {:pre [(vector? items)]}
  (impl-list :list-or items))

(defonce/record Date [date]
  MessagePattern
  (localize [_ state]
    (.format ^DateFormat (.-date ^LocalizationState state) date)))

(defn date
  "Create a date message pattern (e.g. 2025-09-17 or 09/17/25)

  To actually localize, invoke localization (or its state) with pattern

  Args:
    date    either Calendar, Date, Number, or Temporal"
  [date]
  (->Date date))

;; for tests
(defn await-for-updates [^Localization localization]
  (let [p (promise)]
    (send (.-agent localization) (fn [a] (deliver p nil) a))
    @p))

;; TODO:
;;  - editor.progress
;;  - loading project dialog
;;  - editor scripts localization
;;  - menus
;;  - all dialogs
;;  - all properties
;;  - missed things...