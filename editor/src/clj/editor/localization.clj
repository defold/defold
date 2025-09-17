(ns editor.localization
  (:require [camel-snake-kebab :as camel]
            [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code.lang.java-properties :as java-properties]
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
  (:import [clojure.lang AFn IFn]
           [com.ibm.icu.text DateFormat ListFormatter ListFormatter$Type ListFormatter$Width MessageFormat]
           [com.ibm.icu.util ULocale]
           [java.nio.file StandardWatchEventKinds WatchEvent$Kind]
           [java.util Collection WeakHashMap]
           [javafx.application Platform]
           [javafx.scene.control Labeled]
           [javafx.scene.text Text]
           [org.apache.commons.io FilenameUtils]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

;; Implementation note. Localization system uses an agent with different
;; executors used for different tasks:
;; - reload of localization is performed using a send-off (potentially blocking)
;;   executor because it requires reading files from the file system
;; - refresh of listeners is performed using javafx executor because the
;;   listeners are expected to be JavaFX nodes.
;; Localization data shape:
;; (agent
;;   {;; prefs, never changes, used for reading/storing selected locale
;;    :prefs prefs-instance
;;
;;    ;; The following fields may be modified using public API:
;;    ;; - current locale selection, e.g. "en" or "en_US"; settable
;;    :locale "en"
;;    ;; - a map of localization bundles; settable
;;    :bundles {bundle-key {resource-path-str fn:evaluation-context->java.io.Reader}}
;;    ;; - a WeakHashMap (objects are weakly referenced: no need to clean up listeners)
;;    :listeners {object MessagePattern}
;;
;;    ;; The following fields are updated on locale and bundle change:
;;    ;; - message map for the current locale
;;    :messages {message-key fn:arg->string}
;;    ;; - for UI: sorted list of supported locales, sourced from registered
;;    ;;   bundles
;;    :available-locales ["en" "sv"]
;;    ;; - locale-specific formatter of lists like `a, b, and c`
;;    :list-and com.ibm.icu.text.ListFormatter
;;    ;; - locale-specific formatter of lists like `a, b, or c`
;;    :list-or com.ibm.icu.text.ListFormatter
;;    ;; - locale-specific date formatter (e.g. 2025-09-17 or 9/17/25)
;;    :date com.ibm.icu.text.DateFormat})

(defonce/protocol Localizable
  (apply-localization [obj str] "Apply a localization string to object"))

(extend-protocol Localizable
  Labeled
  (apply-localization [label str]
    (.setText label str))
  Text
  (apply-localization [text str]
    (.setText text str))
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

(defn- refresh-messages! [state]
  (let [u-locale (ULocale/createCanonical ^String (:locale state))
        u-locale-chain (vec
                         (e/distinct
                           (e/conj
                             (->> u-locale
                                  (iterate ^[] ULocale/.getFallback)
                                  (e/take-while #(not (coll/empty? (str %)))))
                             (ULocale/createCanonical "en"))))
        u-locale->reader-fns (->> state
                                  :bundles
                                  (e/mapcat val)
                                  (util/group-into
                                    {} []
                                    #(ULocale/createCanonical (FilenameUtils/getBaseName (key %)))
                                    val))
        ;; messages
        evaluation-context (g/make-auto-or-fake-evaluation-context)
        messages (->> u-locale-chain
                      (e/mapcat (fn [u-locale]
                                  (when-let [reader-fns (u-locale->reader-fns u-locale)]
                                    (->> reader-fns
                                         (e/mapcat #(java-properties/parse (% evaluation-context)))
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
    (when-not (:fake evaluation-context)
      (if (Platform/isFxApplicationThread)
        (g/update-cache-from-evaluation-context! evaluation-context)
        (ui/run-later (g/update-cache-from-evaluation-context! evaluation-context))))
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

(defonce/type Localization [agent]
  IFn
  (invoke [_ v]
    (if (message-pattern? v)
      (.localize ^MessagePattern v @agent)
      (str v)))
  (applyTo [this args]
    (AFn/applyToHelper this args)))

(defn- localization? [x]
  (instance? Localization x))

(defn- impl-set-bundle! [state key bundle]
  (-> state
      (update :bundles assoc key bundle)
      refresh-messages!))

(defn- impl-set-locale! [state locale]
  (prefs/set! (:prefs state) [:window :locale] locale)
  (-> state
      (assoc :locale locale)
      refresh-messages!))

(defn- impl-localize! [state object message-pattern]
  (assert (Platform/isFxApplicationThread))
  (.put ^WeakHashMap (:listeners state) object message-pattern)
  state)

(defn- refresh-listeners! [state]
  (assert (Platform/isFxApplicationThread))
  (.forEach ^WeakHashMap (:listeners state) #(apply-localization %1 (.localize ^MessagePattern %2 state)))
  state)

(defn available-locales
  "Get a list of available locales that have translations

  Args:
    localization    the localization instance created with [[make]]"
  [^Localization localization]
  {:pre [(localization? localization)]}
  (:available-locales @(.-agent localization)))

(defn current-locale
  "Get current locale (a string)

  Args:
    localization    the localization instance created with [[make]]"
  [^Localization localization]
  {:pre [(localization? localization)]}
  (:locale @(.-agent localization)))

(defn localize
  "Produce a localized string from a pattern; same as invoking localization

  Args:
    localization    the localization instance created with [[make]]
    pattern         any value, but preferably a MessagePattern instance, created
                    with e.g. [[message]] or [[list-and]]"
  [localization pattern]
  {:pre [(localization? localization)]}
  (localization pattern))

(defn set-bundle!
  "Asynchronously update localization bundle

  Args:
    localization    the localization instance created with [[make]]
    bundle-key      bundle identifier, e.g. a keyword
    bundle          the bundle map, a map from localization file paths to 1-arg
                    functions that receive evaluation context and return a
                    reader for java properties file"
  [^Localization localization bundle-key bundle]
  {:pre [(localization? localization)]}
  (send-off (.-agent localization) impl-set-bundle! bundle-key bundle)
  (send-via ui/javafx-executor (.-agent localization) refresh-listeners!))

(defn set-locale!
  "Asynchronously det the active locale and notify listeners

  Args:
    localization    the localization instance created with [[make]]
    locale          a locale identifier string (e.g. \"en\", \"en_US\")

  Persists the selected locale in user prefs and refreshes all
  registered listeners on the JavaFX thread."
  [^Localization localization locale]
  {:pre [(localization? localization) (string? locale)]}
  (send-off (.-agent localization) impl-set-locale! locale)
  (send-via ui/javafx-executor (.-agent localization) refresh-listeners!))

(defn localize!
  "Localize an object that implements Localizable protocol

  Immediately applies localization to the object with the current message value;
  and does it again on JavaFX thread whenever the locale or localization bundle
  change. The object will be stored in a weak reference: there is no need to
  unregister the localization listener.

  Returns the passed object

  Args:
    object          target object, will be stored in a weak reference
    localization    the localization instance created with [[make]]
    pattern         a MessagePattern instance, created with e.g. [[message]]"
  ([object ^Localization localization pattern]
   {:pre [(some? object) (localization? localization) (message-pattern? pattern)]}
   (apply-localization object (localization pattern))
   (send-via ui/javafx-executor (.-agent localization) impl-localize! object pattern)
   object))

(defn make
  "Create a localization instance (agent) with initial bundle

  Args:
    prefs                  preferences handle used to persist locale selection
    initial-bundle-key     identifier for the initial bundle (e.g. a keyword)
    initial-bundle         initial bundle map, a map from localization file
                           paths to 1-arg functions that receive evaluation
                           context and return a reader for java properties file

  Returns a localization system agent"
  [prefs initial-bundle-key initial-bundle]
  (->Localization
    (agent
      (impl-set-bundle!
        {:prefs prefs
         :locale (prefs/get prefs [:window :locale])
         :bundles {}
         :listeners (WeakHashMap.)}
        initial-bundle-key initial-bundle)
      :error-handler (fn report-localization-error [_ exception]
                       (error-reporting/report-exception! exception)))))

(defn make-editor [prefs]
  (letfn [(get-bundle [resource-dir]
            (->> resource-dir
                 (fs/class-path-walker java/class-loader)
                 (e/filter #(.endsWith (str %) ".editor_localization"))
                 (coll/pair-map-by
                   str
                   (fn [path]
                     ;; We can read Path only during iteration (because we open
                     ;; the zip file), but the actual reading is done later,
                     ;; during reload. Converting path to a URL allows reading
                     ;; from the zip file later.
                     (let [url (.toURL (.toUri (fs/path path)))]
                       (fn [_evaluation-context]
                         (io/reader url)))))))]
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

(defn- impl-simple-message [k m state]
  (if-let [format-fn (get (:messages state) k)]
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

  To actually localize, use [[localize]] or invoke localization with pattern

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

  To actually localize, use [[localize]] or invoke localization with pattern

  Args:
    items    vector of items, either strings, numbers, or other localizable
             messages"
  [items]
  {:pre [(vector? items)]}
  (impl-list :list-and items))

(defn or-list
  "Create a list message pattern using \"or\" conjunction (e.g., a, b, or c)

  To actually localize, use [[localize]] or invoke localization with pattern

  Args:
    items    vector of items, either strings, numbers, or other localizable
             messages"
  [items]
  {:pre [(vector? items)]}
  (impl-list :list-or items))

(defonce/record Date [date]
  MessagePattern
  (localize [_ state]
    (.format ^DateFormat (:date state) date)))

(defn date
  "Create a date message pattern (e.g. 2025-09-17 or 09/17/25)

  To actually localize, use [[localize]] or invoke localization with pattern

  Args:
    date    either Calendar, Date, Number, or Temporal"
  [date]
  (->Date date))

;; for tests
(defn await-for-updates [^Localization localization]
  (let [p (promise)]
    (send (.-agent localization) (fn [a] (deliver p nil) a))
    @p))

;; TODO list
;;  - in-project localization
;;  - editor.progress
;;  - loading project dialog
;;  - editor script localization

(comment

  ((message "bar" {"a" 1 "v" (or-list
                               [(message "foo")
                                "a"
                                "b"])})
   @(make (dev/prefs) ::editor {"/en.txt" (constantly (java.io.StringReader. "bar = {a} with v = {v}\nfoo = Ohh mammaaa"))}))

  #__)

