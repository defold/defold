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
(ns editor.app-manifest
  (:require [dynamo.graph :as g]
            [editor.code.data :as data]
            [editor.code.resource :as r]
            [editor.code.util :as util]
            [editor.graph-util :as gu]
            [editor.localization :as localization]
            [editor.properties :as properties]
            [editor.resource-io :as resource-io]
            [editor.yaml :as yaml]))

(def macos #{:x86_64-osx :arm64-osx})

(def windows #{:x86-win32 :x86_64-win32})

(def android #{:armv7-android :arm64-android})

(def ios #{:armv7-ios :arm64-ios :x86_64-ios})

(def web #{:js-web :wasm-web :wasm_pthread-web})

(def linux #{:x86_64-linux :arm64-linux})

(def vulkan
  #{:x86_64-linux :arm64-linux
    :x86-win32 :x86_64-win32
    :armv7-android :arm64-android
    :arm64-ios})

(def vulkan-osx #{:x86_64-osx :arm64-osx})

(def all-platforms
  #{;; ios
    :armv7-ios :arm64-ios :x86_64-ios
    ;; android
    :armv7-android :arm64-android
    ;; osx
    :x86_64-osx :arm64-osx
    ;; linux
    :x86_64-linux :arm64-linux
    ;; windows
    :x86-win32 :x86_64-win32
    ;; web
    :js-web :wasm-web :wasm_pthread-web})

(def custom-lib-names
  {:x86-win32 {"hid" "hid"
               "hid_null" "hid_null"
               "input" "input"
               "platform" "platform"
               "platform_null" "platform_null"
               "platform_vulkan" "platform_vulkan"
               "vpx" "vpx"
               "vulkan" "vulkan-1"}
   :x86_64-win32 {"hid" "hid"
                  "hid_null" "hid_null"
                  "input" "input"
                  "platform" "platform"
                  "platform_null" "platform_null"
                  "platform_vulkan" "platform_vulkan"
                  "vpx" "vpx"
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

(defn make-choice-setting [& id+toggles-then-none]
  (let [none (last id+toggles-then-none)
        choices (into [] (partition-all 2) (butlast id+toggles-then-none))]
    (assert (or (keyword? none) (map? none)))
    (assert (every? (fn [[id toggles]]
                      (and (or (keyword? id) (map? id)) (coll? toggles) (every? map? toggles)))
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
          first-fit-choice (some (fn [[_id toggles :as choice]]
                                   (when (properties/unify-values (mapv #(get-toggle-value manifest %) toggles))
                                     choice))
                                 choices)]
      (if first-fit-choice
        (let [[choice-id choice-toggles] first-fit-choice
              choice-toggle? (set choice-toggles)
              remaining-toggle-values (into []
                                            (comp
                                              (mapcat second)
                                              (remove choice-toggle?)
                                              (map #(get-toggle-value manifest %)))
                                            choices)]
          (if (or (zero? (count remaining-toggle-values))
                  (false? (properties/unify-values remaining-toggle-values)))
            choice-id
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
                                    (some (fn [[id toggles]]
                                            (when (= id value) toggles))
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

(defn update-setting-value [manifest setting f & args]
  (let [v (get-setting-value manifest setting)
        v (if (nil? v)
            (case (:setting setting)
              :check-box false
              :choice (:none setting))
            v)]
    (set-setting-value manifest setting (apply f v args))))

(defn setting-property-updater [setting f & args]
  (fn [_evaluation-context self old new]
    (when-not (g/error? old)
      (apply g/update-property self :manifest update-setting-value setting f (concat args [new])))))

;; endregion

(def record-setting
  (make-check-box-setting
    (concat
      (exclude-libs-toggles all-platforms ["record" "vpx"])
      (libs-toggles all-platforms ["record_null"]))))

(def profiler-setting
  (let [none-toggles (concat
                       (libs-toggles all-platforms ["profile_null", "profilerext_null"])
                       (generic-contains-toggles all-platforms :excludeSymbols ["ProfilerBasic", "ProfilerRemotery", "ProfilerJS"])
                       (exclude-libs-toggles all-platforms ["profile", "profilerext", "profiler_remotery", "profiler_js"]))
        always-toggles (concat
                         (exclude-libs-toggles all-platforms ["profile_null", "profilerext_null"])
                         (generic-contains-toggles all-platforms :symbols ["ProfilerExt" "ProfilerBasic"])
                         (generic-contains-toggles windows :symbols ["ProfilerRemotery"])
                         (generic-contains-toggles macos :symbols ["ProfilerRemotery"])
                         (generic-contains-toggles linux :symbols ["ProfilerRemotery"])
                         (generic-contains-toggles android :symbols ["ProfilerRemotery"])
                         (generic-contains-toggles ios :symbols ["ProfilerRemotery"])
                         (generic-contains-toggles web :symbols ["ProfilerJS"])
                         (libs-toggles all-platforms ["profile", "profilerext"])
                         (libs-toggles windows ["profiler_remotery"])
                         (libs-toggles macos ["profiler_remotery"])
                         (libs-toggles linux ["profiler_remotery"])
                         (libs-toggles android ["profiler_remotery"])
                         (libs-toggles ios ["profiler_remotery"])
                         (libs-toggles web ["profiler_js"]))]
    (make-choice-setting
      :none none-toggles
      :always always-toggles
      :debug-only)))

(def font-setting
  (make-check-box-setting
    (concat
      (exclude-libs-toggles all-platforms ["font"])
      (libs-toggles all-platforms ["font_skribidi", "harfbuzz", "sheenbidi", "unibreak", "skribidi"]))))

(def sound-setting
  (make-check-box-setting
    (concat
      (exclude-libs-toggles all-platforms ["sound" "tremolo"])
      (generic-contains-toggles all-platforms :excludeSymbols ["DefaultSoundDevice" "AudioDecoderWav" "AudioDecoderStbVorbis" "AudioDecoderTremolo"])
      (libs-toggles all-platforms ["sound_null"]))))

(def sound-decoder-wav-setting
  (make-check-box-setting
    (concat
      (exclude-libs-toggles all-platforms ["decoder_wav"])
      (generic-contains-toggles all-platforms :excludeSymbols ["AudioDecoderWav" "ResourceTypeWav"]))))

(def sound-decoder-ogg-setting
  (make-check-box-setting
    (concat
      (exclude-libs-toggles all-platforms ["decoder_ogg"])
      (generic-contains-toggles all-platforms :excludeSymbols ["AudioDecoderStbVorbis" "AudioDecoderTremolo" "ResourceTypeOgg"]))))

(def sound-decoder-opus-setting
  (make-check-box-setting
    (concat
      (libs-toggles all-platforms ["decoder_opus" "opus"])
      (generic-contains-toggles all-platforms :symbols ["AudioDecoderOpus" "ResourceTypeOpus"]))))

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

(def types-setting
  (make-check-box-setting
    (concat
      (generic-contains-toggles all-platforms :excludeSymbols ["ScriptTypesExt"]))))

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
  ;; by default, legacy 2d and 3d are included in `physics` lib
  (let [;; must be used when excluding anything
        exclude-default (exclude-libs-toggles all-platforms ["physics"])

        ;; must use at least one of these when excluding default
        exclude-3d (exclude-libs-toggles all-platforms ["LinearMath" "BulletDynamics" "BulletCollision"])
        exclude-legacy-2d (exclude-libs-toggles all-platforms ["box2d_defold" "script_box2d_defold"])

        ;; must be used when excluding 2d completely:
        exclude-all-2d (generic-contains-toggles all-platforms :excludeSymbols ["ScriptBox2DExt"])

        ;; must be used when excluding all physics:
        exclude-all (libs-toggles all-platforms ["physics_null"])

        ;; can only be used when using exclude-default:
        include-legacy-2d (libs-toggles all-platforms ["physics_2d_defold"])
        include-2d-v3 (libs-toggles all-platforms ["physics_2d" "box2d" "script_box2d"])
        include-3d (libs-toggles all-platforms ["physics_3d"])]
    (make-choice-setting
      {:2d :none :3d false}
      (concat exclude-all exclude-default exclude-3d exclude-legacy-2d exclude-all-2d)

      {:2d :legacy :3d false}
      (concat exclude-default exclude-3d include-legacy-2d)

      {:2d :v3 :3d false}
      (concat exclude-default exclude-3d exclude-legacy-2d include-2d-v3)

      {:2d :none :3d true}
      (concat exclude-default exclude-legacy-2d exclude-all-2d include-3d)

      {:2d :v3 :3d true}
      (concat exclude-default exclude-legacy-2d include-2d-v3 include-3d)

      {:2d :legacy :3d true})))

(def image-setting
  (make-check-box-setting
    (concat
      (exclude-libs-toggles all-platforms ["image"])
      (libs-toggles all-platforms ["image_null"])
      (generic-contains-toggles all-platforms :excludeSymbols ["ScriptImageExt"]))))

(def rig-setting
  (make-choice-setting
    :none (concat (libs-toggles all-platforms ["gamesys_rig_null" "gamesys_model_null" "rig_null"]) (exclude-libs-toggles all-platforms ["gamesys_model" "gamesys_rig" "rig"]) (generic-contains-toggles all-platforms :excludeSymbols ["ScriptModelExt"]))
    :rig   (concat (libs-toggles all-platforms ["gamesys_model_null"])   (exclude-libs-toggles all-platforms ["gamesys_model"]) (generic-contains-toggles all-platforms :excludeSymbols ["ScriptModelExt"]))
    :model))


(def vulkan-toggles
  (concat
    (exclude-libs-toggles [:x86-win32 :x86_64-win32] ["platform"])
    (libs-toggles [:x86-win32 :x86_64-win32 :arm64-linux :x86_64-linux] ["platform_vulkan"])
    (libs-toggles [:arm64-ios] ["graphics_vulkan" "MoltenVK"])
    (libs-toggles android ["graphics_vulkan"])
    (libs-toggles windows ["graphics_vulkan" "vulkan"])
    (libs-toggles linux ["graphics_vulkan" "X11-xcb"])
    (generic-contains-toggles linux :dynamicLibs ["vulkan"])
    (generic-contains-toggles [:arm64-ios] :frameworks ["Metal" "IOSurface" "QuartzCore"])
    (generic-contains-toggles vulkan :symbols ["GraphicsAdapterVulkan"])))

(def graphics-setting
  (make-choice-setting
    :vulkan (concat
              vulkan-toggles
              (exclude-libs-toggles vulkan ["graphics"])
              (generic-contains-toggles (disj vulkan :arm64-linux) :excludeSymbols ["GraphicsAdapterOpenGL"])
              [(contains-toggle :arm64-linux :excludeSymbols "GraphicsAdapterOpenGLES")])
    :both vulkan-toggles
    :open-gl))

(def open-gl-osx-toggles
  (concat
    (libs-toggles vulkan-osx ["graphics" "platform"])
    (generic-contains-toggles vulkan-osx :symbols ["GraphicsAdapterOpenGL"])
    (generic-contains-toggles vulkan-osx :frameworks ["OpenGL"])))

(def graphics-setting-osx
  (make-choice-setting
    :open-gl (concat
               open-gl-osx-toggles
               (exclude-libs-toggles vulkan-osx ["graphics_vulkan" "platform_vulkan" "MoltenVK"])
               (generic-contains-toggles vulkan-osx :excludeSymbols ["GraphicsAdapterVulkan"]))
    :both open-gl-osx-toggles
    :vulkan))

(def webgpu-toggles
  (concat
    (libs-toggles web ["graphics_webgpu"])
    (generic-contains-toggles web :symbols ["GraphicsAdapterWebGPU"])
    (generic-contains-toggles web :emscriptenLinkFlags ["USE_WEBGPU=1" "GL_WORKAROUND_SAFARI_GETCONTEXT_BUG=0"])
    (generic-contains-toggles [:wasm-web] :emscriptenLinkFlags ["ASYNCIFY=1" "ASYNCIFY_IGNORE_INDIRECT=1" "ASYNCIFY_ADD=[\"main\",\"dmEngineCreate(*)\",\"requestDeviceCallback(*)\",\"WebGPUCreateSwapchain(*)\",\"instanceRequestAdapterCallback(*)\"]"])
    (generic-contains-toggles [:wasm_pthread-web] :emscriptenLinkFlags ["ASYNCIFY=1" "ASYNCIFY_IGNORE_INDIRECT=1" "ASYNCIFY_ADD=[\"main\",\"dmEngineCreate(*)\",\"requestDeviceCallback(*)\",\"WebGPUCreateSwapchain(*)\",\"instanceRequestAdapterCallback(*)\"]" "PTHREAD_POOL_SIZE=1"])))

(def graphics-web-setting
  (make-choice-setting
    :web-gpu (concat
               webgpu-toggles
              (exclude-libs-toggles web ["graphics"])
              (generic-contains-toggles web :excludeSymbols ["GraphicsAdapterOpenGL"]))
    :both webgpu-toggles
    :web-gl))

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
                  [:arm64-osx platform-pattern]
                  [:x86_64-osx platform-pattern]
                  ;; linux
                  [:x86_64-linux platform-pattern]
                  [:arm64-linux platform-pattern]
                  ;; windows
                  [:x86-win32 platform-pattern]
                  [:x86_64-win32 platform-pattern]
                  ;; web
                  [:js-web platform-pattern]
                  [:wasm-web platform-pattern]
                  [:wasm_pthread-web platform-pattern]]]]))

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
  (property physics-2d g/Any
            (dynamic label (properties/label-dynamic :appmanifest :physics-2d))
            (dynamic tooltip (properties/tooltip-dynamic :appmanifest :physics-2d))
            (dynamic edit-type (g/constantly {:type :choicebox
                                              :options [[:v3 "Box2D Version 3"]
                                                        [:legacy "Box2D (Legacy Defold version)"]
                                                        [:none "None"]]}))
            (value (g/fnk [manifest] (:2d (get-setting-value manifest physics-setting))))
            (set (setting-property-updater physics-setting assoc :2d)))
  (property physics-3d g/Any
            (dynamic label (properties/label-dynamic :appmanifest :physics-3d))
            (dynamic tooltip (properties/tooltip-dynamic :appmanifest :physics-3d))
            (dynamic edit-type (g/constantly {:type g/Bool}))
            (value (g/fnk [manifest] (:3d (get-setting-value manifest physics-setting))))
            (set (setting-property-updater physics-setting assoc :3d)))
  (property rig+model g/Any
            (dynamic label (properties/label-dynamic :appmanifest :rig+model))
            (dynamic tooltip (properties/tooltip-dynamic :appmanifest :rig+model))
            (dynamic edit-type (g/constantly {:type :choicebox
                                              :options [[:model "Rig & Model"]
                                                        [:rig "Rig only"]
                                                        [:none "None"]]}))
            (value (setting-property-getter rig-setting))
            (set (setting-property-setter rig-setting)))
  (property exclude-record g/Any
            (dynamic label (properties/label-dynamic :appmanifest :exclude-record))
            (dynamic tooltip (properties/tooltip-dynamic :appmanifest :exclude-record))
            (dynamic edit-type (g/constantly {:type g/Bool}))
            (value (setting-property-getter record-setting))
            (set (setting-property-setter record-setting)))
  (property profiler g/Any
            (dynamic label (properties/label-dynamic :appmanifest :profiler))
            (dynamic tooltip (properties/tooltip-dynamic :appmanifest :profiler))
            (dynamic edit-type (g/constantly {:type :choicebox
                                              :options [[:debug-only "Debug Only"]
                                                        [:none "None"]
                                                        [:always "Always"]]}))
            (value (setting-property-getter profiler-setting))
            (set (setting-property-setter profiler-setting)))
  (property exclude-sound g/Any
            (dynamic label (properties/label-dynamic :appmanifest :exclude-sound))
            (dynamic tooltip (properties/tooltip-dynamic :appmanifest :exclude-sound))
            (dynamic edit-type (g/constantly {:type g/Bool}))
            (value (setting-property-getter sound-setting))
            (set (setting-property-setter sound-setting)))
  (property exclude-sound-decoder-wav g/Any
            (dynamic label (properties/label-dynamic :appmanifest :exclude-sound-decoder-wav))
            (dynamic tooltip (properties/tooltip-dynamic :appmanifest :exclude-sound-decoder-wav))
            (dynamic edit-type (g/constantly {:type g/Bool}))
            (value (setting-property-getter sound-decoder-wav-setting))
            (set (setting-property-setter sound-decoder-wav-setting)))
  (property exclude-sound-decoder-ogg g/Any
            (dynamic label (properties/label-dynamic :appmanifest :exclude-sound-decoder-ogg))
            (dynamic tooltip (properties/tooltip-dynamic :appmanifest :exclude-sound-decoder-ogg))
            (dynamic edit-type (g/constantly {:type g/Bool}))
            (value (setting-property-getter sound-decoder-ogg-setting))
            (set (setting-property-setter sound-decoder-ogg-setting)))
  (property include-sound-decoder-opus g/Any
            (dynamic label (properties/label-dynamic :appmanifest :include-sound-decoder-opus))
            (dynamic tooltip (properties/tooltip-dynamic :appmanifest :include-sound-decoder-opus))
            (dynamic edit-type (g/constantly {:type g/Bool}))
            (value (setting-property-getter sound-decoder-opus-setting))
            (set (setting-property-setter sound-decoder-opus-setting)))
  (property exclude-input g/Any
            (dynamic label (properties/label-dynamic :appmanifest :exclude-input))
            (dynamic tooltip (properties/tooltip-dynamic :appmanifest :exclude-input))
            (dynamic edit-type (g/constantly {:type g/Bool}))
            (value (setting-property-getter input-setting))
            (set (setting-property-setter input-setting)))
  (property exclude-liveupdate g/Any
            (dynamic label (properties/label-dynamic :appmanifest :exclude-liveupdate))
            (dynamic tooltip (properties/tooltip-dynamic :appmanifest :exclude-liveupdate))
            (dynamic edit-type (g/constantly {:type g/Bool}))
            (value (setting-property-getter liveupdate-setting))
            (set (setting-property-setter liveupdate-setting)))
  (property exclude-image g/Any
            (dynamic label (properties/label-dynamic :appmanifest :exclude-image))
            (dynamic tooltip (properties/tooltip-dynamic :appmanifest :exclude-image))
            (dynamic edit-type (g/constantly {:type g/Bool}))
            (value (setting-property-getter image-setting))
            (set (setting-property-setter image-setting)))
  (property exclude-types g/Any
            (dynamic label (properties/label-dynamic :appmanifest :exclude-types))
            (dynamic tooltip (properties/tooltip-dynamic :appmanifest :exclude-types))
            (dynamic edit-type (g/constantly {:type g/Bool}))
            (value (setting-property-getter types-setting))
            (set (setting-property-setter types-setting)))
  (property exclude-basis-transcoder g/Any
            (dynamic label (properties/label-dynamic :appmanifest :exclude-basis-transcoder))
            (dynamic tooltip (properties/tooltip-dynamic :appmanifest :exclude-basis-transcoder))
            (dynamic edit-type (g/constantly {:type g/Bool}))
            (value (setting-property-getter basis-transcoder-setting))
            (set (setting-property-setter basis-transcoder-setting)))
  (property use-android-support-lib g/Any
            (dynamic label (properties/label-dynamic :appmanifest :use-android-support-lib))
            (dynamic tooltip (properties/tooltip-dynamic :appmanifest :use-android-support-lib))
            (dynamic edit-type (g/constantly {:type g/Bool}))
            (value (setting-property-getter use-android-support-lib-setting))
            (set (setting-property-setter use-android-support-lib-setting)))
  (property graphics g/Any
            (dynamic label (properties/label-dynamic :appmanifest :graphics))
            (dynamic tooltip (properties/tooltip-dynamic :appmanifest :graphics))
            (dynamic edit-type (g/constantly {:type :choicebox
                                              :options [[:open-gl "OpenGL"]
                                                        [:vulkan "Vulkan"]
                                                        [:both "OpenGL & Vulkan"]]}))
            (value (setting-property-getter graphics-setting))
            (set (setting-property-setter graphics-setting)))
  (property graphics-osx g/Any
            (dynamic label (properties/label-dynamic :appmanifest :graphics-osx))
            (dynamic tooltip (properties/tooltip-dynamic :appmanifest :graphics-osx))
            (dynamic edit-type (g/constantly {:type :choicebox
                                              :options [[:vulkan "Vulkan"]
                                                        [:open-gl "OpenGL"]
                                                        [:both "OpenGL & Vulkan"]]}))
            (value (setting-property-getter graphics-setting-osx))
            (set (setting-property-setter graphics-setting-osx)))
  (property graphics-web g/Any
            (dynamic label (properties/label-dynamic :appmanifest :graphics-web))
            (dynamic tooltip (properties/tooltip-dynamic :appmanifest :graphics-web))
            (dynamic edit-type (g/constantly {:type :choicebox
                                              :options [[:web-gl "WebGL"]
                                                        [:web-gpu "WebGPU"]
                                                        [:both "WebGL & WebGPU"]]}))
            (value (setting-property-getter graphics-web-setting))
            (set (setting-property-setter graphics-web-setting)))
  (property use-font-layout g/Any
            (dynamic label (properties/label-dynamic :appmanifest :use-font-layout))
            (dynamic tooltip (properties/tooltip-dynamic :appmanifest :use-font-layout))
            (dynamic edit-type (g/constantly {:type g/Bool}))
            (value (setting-property-getter font-setting))
            (set (setting-property-setter font-setting))))

(defn register-resource-types [workspace]
  (r/register-code-resource-type
    workspace
    :ext "appmanifest"
    :language "yaml"
    :label (localization/message "resource.type.appmanifest")
    :icon "icons/32/Icons_05-Project-info.png"
    :node-type AppManifestNode
    :view-types [:code :default]
    :view-opts {:code {:use-custom-editor false}}
    :lazy-loaded true))
