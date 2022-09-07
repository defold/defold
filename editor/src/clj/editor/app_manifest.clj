;; Copyright 2020-2022 The Defold Foundation
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
(ns editor.app-manifest
  (:require [dynamo.graph :as g]
            [editor.code.data :as data]
            [editor.code.resource :as r]
            [editor.code.util :as util]
            [editor.graph-util :as gu]
            [editor.properties :as properties]
            [editor.resource-io :as resource-io]
            [editor.yaml :as yaml]))

(def windows #{:x86-win32 :x86_64-win32})

(def android #{:armv7-android :arm64-android})

(def linux #{:x86_64-linux})

(def vulkan
  #{:x86_64-osx
    :x86_64-linux
    :x86-win32 :x86_64-win32
    :armv7-android :arm64-android
    :arm64-ios})

(def all-platforms
  #{;; ios
    :armv7-ios :arm64-ios :x86_64-ios
    ;; android
    :armv7-android :arm64-android
    ;; osx
    :x86_64-osx
    ;; linux
    :x86_64-linux
    ;; windows
    :x86-win32 :x86_64-win32
    ;; web
    :js-web :wasm-web})

(def custom-lib-names
  {:x86-win32 {"vpx" "vpx"
               "vulkan" "vulkan-1"}
   :x86_64-win32 {"vpx" "vpx"
                  "vulkan" "vulkan-1"}})

(defn platformify-excluded-lib [platform lib]
  (or (-> custom-lib-names platform (get lib))
      (and (contains? windows platform) (str "lib" lib))
      lib))

(defn platformify-lib [platform lib]
  (or (some-> custom-lib-names platform (get lib) (str ".lib"))
      (and (contains? windows platform) (str "lib" lib ".lib"))
      lib))

;; region toggles

(defn contains-toggle [platform key value]
  {:toggle :contains :platform platform :key key :value value})

(defn boolean-toggle [platform key value]
  {:toggle :boolean :platform platform :key key :value value})

(defn exclude-libs-toggles [platforms libs]
  (for [p platforms
        l libs]
    (contains-toggle p :excludeLibs (platformify-excluded-lib p l))))

(defn libs-toggles [platforms libs]
  (for [p platforms
        l libs]
    (contains-toggle p :libs (platformify-lib p l))))

(defn generic-contains-toggles [platforms key values]
  (for [p platforms
        v values]
    (contains-toggle p key v)))

(defmacro ^{:arglists '([m & k+pred* default?])} get-in-guarded
  "Get value from nested map, performing an extra check after every get

  When either the key is absent or the check fails, the optional default value
  is returned

  Example:

    (get-in-guarded {:a 1} :a boolean?) => nil
    (get-in-guarded {:a 1} :a boolean? false) => false
    (get-in-guarded {:a true} :a boolean? false) => true"
  [m & args]
  (let [has-default (odd? (count args))
        default (if has-default (last args) nil)
        k+pred-pairs (partition 2 (if has-default (butlast args) args))
        m-sym (gensym "m")
        default-sym (gensym "default")]
    `(let [~m-sym ~m
           ~default-sym ~default]
       ~(reduce (fn [form [k pred]]
                  `(let [~m-sym (get ~m-sym ~k ::not-found)]
                     (if (or (identical? ::not-found ~m-sym)
                             (not (~pred ~m-sym)))
                       ~default-sym
                       ~form)))
                m-sym
                (reverse k+pred-pairs)))))

(defmacro ^{:arglists '([m pred fix & k+pred+fix* fn])} update-in-fixing
  "Update value in a nested map while performing nested checks + substitutions

  Args are:
  - a triplet of initial expr, pred, fix if pred fails
  - 0 or more triplets of key pred fix
  - fn that changes the value in a nested map

  Example:

    (update-in-fixing
      1 map? {}
      :foo number? 0
      inc)
    => {:foo 1}

    (update-in-fixing
      {:foo true} map? {}
      :foo number? 0
      inc)
    => {:foo 1}

    (update-in-fixing
      {:foo 1} map? {}
      :foo number? 0
      inc)
    => {:foo 2}"
  [m pred fix & args]
  {:pre [(= 1 (rem (count args) 3))]}
  (let [f-expr (last args)
        forms (partition 3 (butlast args))
        value-sym (gensym "value")
        map-sym (gensym "map")]
    `(let [~value-sym ~m
           ~value-sym (if (not (~pred ~value-sym))
                        ~fix
                        ~value-sym)]
      ~(reduce
         (fn [form [k pred fix]]
           `(let [~map-sym ~value-sym
                  ~value-sym (get ~map-sym ~k ::not-found)
                  ~value-sym (if (or (identical? ~value-sym ::not-found)
                                     (not (~pred ~value-sym)))
                               ~fix
                               ~value-sym)]
              (assoc ~map-sym ~k ~form)))
         `(~f-expr ~value-sym)
         (reverse forms)))))

(defn get-toggle-value [manifest toggle]
  (case (:toggle toggle)
    :contains (let [{:keys [platform key value]} toggle]
                (boolean (some #(= value %) (get-in-guarded manifest
                                                            :platforms map?
                                                            platform map?
                                                            :context map?
                                                            key vector?
                                                            []))))
    :boolean (let [{:keys [platform key value]} toggle
                   default-value (not value)]
               (= value (get-in-guarded manifest
                                        :platforms map?
                                        platform map?
                                        :context map?
                                        key boolean?
                                        default-value)))))

(defn set-toggle-value [manifest toggle value]
  (case (:toggle toggle)
    :contains (let [enabled value
                    {:keys [platform key value]} toggle]
                (update-in-fixing
                  manifest map? {}
                  :platforms map? {}
                  platform map? {}
                  :context map? {}
                  key vector? []
                  (if enabled
                    (fn [values]
                      (into [] (distinct) (conj values value)))
                    (fn [values]
                      (filterv #(not= value %) values)))))

    :boolean (let [enabled value
                   {:keys [platform key value]} toggle]
               (update-in-fixing
                 manifest map? {}
                 :platforms map? {}
                 platform map? {}
                 :context map? {}
                 key boolean? false
                 (constantly (if enabled value (not value)))))))


;; endregion

;; region settings

(defn make-check-box-setting [toggles]
  {:setting :check-box :toggles toggles})

(defn make-choice-setting [& kw+toggles-then-none]
  (let [none (last kw+toggles-then-none)
        choices (into [] (partition-all 2) (butlast kw+toggles-then-none))]
    (assert (keyword? none))
    (assert (every? (fn [[kw toggles]]
                      (and (keyword? kw) (coll? toggles) (every? map? toggles)))
                    choices))
    {:setting :choice
     :choices choices
     :none none}))

(defn get-setting-value [manifest setting]
  (case (:setting setting)
    :check-box
    (->> setting
         :toggles
         (mapv #(get-toggle-value manifest %))
         properties/unify-values)

    :choice
    (let [{:keys [choices none]} setting
          first-fit-choice (some (fn [[_kw toggles :as choice]]
                                   (when (properties/unify-values (mapv #(get-toggle-value manifest %) toggles))
                                     choice))
                                 choices)]
      (if first-fit-choice
        (let [[choice-kw choice-toggles] first-fit-choice
              choice-toggle? (set choice-toggles)
              remaining-toggle-values (into []
                                            (comp
                                              (mapcat second)
                                              (remove choice-toggle?)
                                              (map #(get-toggle-value manifest %)))
                                            choices)]
          (if (or (zero? (count remaining-toggle-values))
                  (false? (properties/unify-values remaining-toggle-values)))
            choice-kw
            nil))
        (let [all-toggles-unify-to-nil (nil? (properties/unify-values
                                               (into []
                                                     (comp
                                                       (mapcat second)
                                                       (map #(get-toggle-value manifest %)))
                                                     choices)))]
         (if all-toggles-unify-to-nil nil none))))))

(defn set-setting-value [manifest setting value]
  (case (:setting setting)
    :check-box (reduce #(set-toggle-value %1 %2 value) manifest (:toggles setting))
    :choice (let [{:keys [choices none]} setting
                  enabled-toggles (if (= none value)
                                    nil
                                    (some (fn [[kw toggles]]
                                            (when (= kw value) toggles))
                                          choices))
                  disabled-toggles (if (= none value)
                                     (mapcat second choices)
                                     (into []
                                           (comp
                                             (remove #(= value (first %)))
                                             (mapcat second))
                                           choices))]
              (as-> manifest $
                    (reduce #(set-toggle-value %1 %2 false) $ disabled-toggles)
                    (reduce #(set-toggle-value %1 %2 true) $ enabled-toggles)))))

(defn setting-property-setter [setting]
  (fn [_evaluation-context self old new]
    (when-not (g/error? old)
      (g/update-property self :manifest set-setting-value setting new))))

(defmacro setting-property-getter [setting-sym]
  `(g/fnk [~'manifest]
     (get-setting-value ~'manifest ~setting-sym)))

;; endregion

(def record-setting
  (make-check-box-setting
    (concat
      (exclude-libs-toggles all-platforms ["record" "vpx"])
      (libs-toggles all-platforms ["record_null"]))))

(def profiler-setting
  (make-check-box-setting
    (concat
      (libs-toggles all-platforms ["profilerext_null"])
      (exclude-libs-toggles all-platforms ["profilerext"])
      (generic-contains-toggles all-platforms :excludeSymbols ["ProfilerExt"]))))

(def sound-setting
  (make-check-box-setting
    (concat
      (exclude-libs-toggles all-platforms ["sound" "tremolo"])
      (generic-contains-toggles all-platforms :excludeSymbols ["DefaultSoundDevice" "AudioDecoderWav" "AudioDecoderStbVorbis" "AudioDecoderTremolo"])
      (libs-toggles all-platforms ["sound_null"]))))

(def input-setting
  (make-check-box-setting
    (concat
      (exclude-libs-toggles all-platforms ["hid"])
      (libs-toggles all-platforms ["hid_null"]))))

(def liveupdate-setting
  (make-check-box-setting
    (concat
      (exclude-libs-toggles all-platforms ["liveupdate"])
      (libs-toggles all-platforms ["liveupdate_null"]))))

(def basis-transcoder-setting
  (make-check-box-setting
    (concat
      (exclude-libs-toggles all-platforms ["graphics_transcoder_basisu" "basis_transcoder"])
      (libs-toggles all-platforms ["graphics_transcoder_null"]))))

(def use-android-support-lib-setting
  (make-check-box-setting
    [(boolean-toggle :armv7-android :jetifier false)
     (boolean-toggle :arm64-android :jetifier false)]))

(def physics-setting
  (make-choice-setting
    :none (concat (libs-toggles all-platforms ["physics_null"]) (exclude-libs-toggles all-platforms ["physics" "LinearMath" "BulletDynamics" "BulletCollision" "Box2D"]))
    :2d   (concat (libs-toggles all-platforms ["physics_2d"])   (exclude-libs-toggles all-platforms ["physics" "LinearMath" "BulletDynamics" "BulletCollision"]))
    :3d   (concat (libs-toggles all-platforms ["physics_3d"])   (exclude-libs-toggles all-platforms ["physics" "Box2D"]))
    :both))

(def vulkan-toggles
  (concat
    (libs-toggles [:x86_64-osx :arm64-ios] ["graphics_vulkan" "MoltenVK"])
    (libs-toggles android ["graphics_vulkan"])
    (libs-toggles windows ["graphics_vulkan" "vulkan"])
    (libs-toggles linux ["graphics_vulkan" "X11-xcb"])
    (generic-contains-toggles [:x86_64-osx] :frameworks ["Metal" "IOSurface" "QuartzCore"])
    (generic-contains-toggles [:arm64-ios] :frameworks ["Metal" "QuartzCore"])
    (generic-contains-toggles vulkan :symbols ["GraphicsAdapterVulkan"])))

(def graphics-setting
  (make-choice-setting
    :vulkan (concat
              vulkan-toggles
              (exclude-libs-toggles vulkan ["graphics"])
              (generic-contains-toggles vulkan :excludeSymbols ["GraphicsAdapterOpenGL"]))
    :both vulkan-toggles
    :open-gl))

(def ^:private app-manifest-key-order-pattern
  (let [platform-pattern [[:context [;; defines
                                     :defines
                                     ;; excludes
                                     :excludeLibs
                                     :excludeJars
                                     :excludeJsLibs
                                     :excludeSymbols
                                     ;; lists
                                     :includes
                                     :symbols
                                     :libs
                                     :dynamicLibs
                                     :engineLibs
                                     :frameworks
                                     :weakFrameworks
                                     :flags
                                     :linkFlags
                                     ;; booleans
                                     :jetifier]]]]
    [[:platforms [;; ios
                  [:armv7-ios platform-pattern]
                  [:arm64-ios platform-pattern]
                  [:x86_64-ios platform-pattern]
                  ;; android
                  [:armv7-android platform-pattern]
                  [:arm64-android platform-pattern]
                  ;; osx
                  [:x86_64-osx platform-pattern]
                  ;; linux
                  [:x86_64-linux platform-pattern]
                  ;; windows
                  [:x86-win32 platform-pattern]
                  [:x86_64-win32 platform-pattern]
                  ;; web
                  [:js-web platform-pattern]
                  [:wasm-web platform-pattern]]]]))

(g/defnode AppManifestNode
  (inherits r/CodeEditorResourceNode)
  (output parsed-manifest g/Any :cached (g/fnk [lines _node-id resource]
                                          (resource-io/with-error-translation resource _node-id :manifest
                                            (yaml/with-error-translation _node-id :manifest resource
                                              (with-open [reader (data/lines-reader lines)]
                                                (yaml/load reader keyword))))))
  (property manifest g/Any
            (dynamic visible (g/constantly false))
            (value (gu/passthrough parsed-manifest))
            (set (fn [evaluation-context self _old-value new-value]
                   (g/set-property
                     self
                     :modified-lines
                     (util/split-lines
                       (yaml/dump new-value
                                  :order-pattern app-manifest-key-order-pattern
                                  :indent (case (g/node-value self :indent-type evaluation-context)
                                            :two-spaces 2
                                            4)))))))
  (property physics g/Any
            (dynamic tooltip (g/constantly "Box2D, Bullet, both or none"))
            (dynamic edit-type (g/constantly {:type :choicebox
                                              :options [[:both "2D & 3D"]
                                                        [:2d "2D"]
                                                        [:3d "3D"]
                                                        [:none "None"]]}))
            (value (setting-property-getter physics-setting))
            (set (setting-property-setter physics-setting)))
  (property exclude-record g/Any
            (dynamic tooltip (g/constantly "Remove the video recording capabilities (desktop platforms)"))
            (dynamic edit-type (g/constantly {:type g/Bool}))
            (value (setting-property-getter record-setting))
            (set (setting-property-setter record-setting)))
  (property exclude-profiler g/Any
            (dynamic tooltip (g/constantly "Remove the on-screen and web profiler"))
            (dynamic edit-type (g/constantly {:type g/Bool}))
            (value (setting-property-getter profiler-setting))
            (set (setting-property-setter profiler-setting)))
  (property exclude-sound g/Any
            (dynamic edit-type (g/constantly {:type g/Bool}))
            (value (setting-property-getter sound-setting))
            (set (setting-property-setter sound-setting)))
  (property exclude-input g/Any
            (dynamic edit-type (g/constantly {:type g/Bool}))
            (value (setting-property-getter input-setting))
            (set (setting-property-setter input-setting)))
  (property exclude-liveupdate g/Any
            (dynamic edit-type (g/constantly {:type g/Bool}))
            (value (setting-property-getter liveupdate-setting))
            (set (setting-property-setter liveupdate-setting)))
  (property exclude-basis-transcoder g/Any
            (dynamic edit-type (g/constantly {:type g/Bool}))
            (value (setting-property-getter basis-transcoder-setting))
            (set (setting-property-setter basis-transcoder-setting)))
  (property use-android-support-lib g/Any
            (dynamic tooltip (g/constantly "Use the old Android support libraries instead of AndroidX. Available from Defold 1.2.177."))
            (dynamic edit-type (g/constantly {:type g/Bool}))
            (value (setting-property-getter use-android-support-lib-setting))
            (set (setting-property-setter use-android-support-lib-setting)))
  (property graphics g/Any
            (dynamic tooltip (g/constantly "Vulkan support is in BETA (desktop and mobile platforms)"))
            (dynamic edit-type (g/constantly {:type :choicebox
                                              :options [[:open-gl "OpenGL"]
                                                        [:vulkan "Vulkan"]
                                                        [:both "OpenGL & Vulkan"]]}))
            (value (setting-property-getter graphics-setting))
            (set (setting-property-setter graphics-setting))))

(defn register-resource-types [workspace]
  (r/register-code-resource-type
    workspace
    :ext "appmanifest"
    :label "App Manifest"
    :icon "icons/32/Icons_05-Project-info.png"
    :node-type AppManifestNode
    :view-types [:code :default]))