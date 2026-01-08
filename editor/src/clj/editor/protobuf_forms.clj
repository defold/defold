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

(ns editor.protobuf-forms
  (:require [clojure.string :as str]
            [dynamo.graph :as g]
            [editor.protobuf :as protobuf]
            [editor.util :as util]
            [util.fn :as fn])
  (:import [com.defold.extension.pipeline.texture ITextureCompressor TextureCompression TextureCompressorUncompressed]
           [com.dynamo.graphics.proto Graphics$PlatformProfile Graphics$PlatformProfile$OS Graphics$TextureImage$TextureFormat Graphics$TextureProfiles]
           [com.dynamo.input.proto Input$Gamepad Input$GamepadMaps Input$GamepadType Input$InputBinding Input$Key Input$Mouse Input$Text Input$Touch]))

(set! *warn-on-reflection* true)

(defn- clear-form-op [{:keys [node-id]} path]
  (g/update-property node-id :pb util/dissoc-in path))

(defn- set-form-op [{:keys [node-id]} path value]
  (g/update-property node-id :pb assoc-in path value))

(defn- longest-prefix-size [enum-values]
  (case (count enum-values)
    (0 1) 0
    (->> enum-values
         (mapv #(-> % first name))
         (apply map hash-set)
         (take-while #(= 1 (count %)))
         count)))

(defn- display-name-or-default [[val {:keys [display-name]}] prefix-size]
  (if (str/blank? display-name)
    (str/join " " (-> val
                      name
                      (subs prefix-size)
                      str/capitalize
                      (str/split #"-")))
    display-name))

(defn make-options [enum-values]
  (let [prefix-size (longest-prefix-size enum-values)]
    (mapv (juxt first #(display-name-or-default % prefix-size)) enum-values)))

(defn- make-enum-options-raw [^Class pb-enum-class]
  (make-options (protobuf/enum-values pb-enum-class)))

(def make-enum-options (fn/memoize make-enum-options-raw))

(defn- default-form-ops [node-id]
  {:form-ops {:user-data {:node-id node-id}
              :set set-form-op
              :clear clear-form-op}})

(defn- make-values [pb keys]
  (into {} (map #(do [[%] (pb %)]) keys)))

(defn- form-values [form-data pb]
  (let [keys (map (comp first :path) (mapcat :fields (:sections form-data)))]
    (make-values pb keys)))

(defmulti protobuf-form-data (fn [_node-id _pb def] (:pb-class def)))

(defmethod protobuf-form-data Input$InputBinding [_node-id _pb _def]
  (let [key-values (butlast (protobuf/enum-values Input$Key)) ; skip MAX_KEY_COUNT
        mouse-values (butlast (protobuf/enum-values Input$Mouse)) ; skip MAX_MOUSE_COUNT
        gamepad-values (butlast (protobuf/enum-values Input$Gamepad)) ; skip MAX_GAMEPAD_COUNT
        touch-values (butlast (protobuf/enum-values Input$Touch)) ; skip MAX_TOUCH_COUNT
        text-values (butlast (protobuf/enum-values Input$Text))] ; skip MAX_TEXT_COUNT
    {:navigation false
     :sections [{:localization-key "input-binding"
                 :fields [{:path [:key-trigger]
                           :localization-key "input-binding.key-trigger"
                           :type :table
                           :columns [{:path [:input]
                                      :localization-key "input-binding.key-trigger.input"
                                      :type :choicebox
                                      :options (sort-by first (make-options key-values))
                                      :default (ffirst key-values)}
                                     {:path [:action]
                                      :localization-key "input-binding.key-trigger.action"
                                      :type :string}]}
                          {:path [:mouse-trigger]
                           :localization-key "input-binding.mouse-trigger"
                           :type :table
                           :columns [{:path [:input]
                                      :localization-key "input-binding.mouse-trigger.input"
                                      :type :choicebox
                                      :options (sort-by first (make-options mouse-values))
                                      :default (ffirst mouse-values)}
                                     {:path [:action]
                                      :localization-key "input-binding.mouse-trigger.action"
                                      :type :string}]}
                          {:path [:gamepad-trigger]
                           :localization-key "input-binding.gamepad-trigger"
                           :type :table
                           :columns [{:path [:input]
                                      :localization-key "input-binding.gamepad-trigger.input"
                                      :type :choicebox
                                      :options (sort-by first (make-options gamepad-values))
                                      :default (ffirst gamepad-values)}
                                     {:path [:action]
                                      :localization-key "input-binding.gamepad-trigger.action"
                                      :type :string}]}
                          {:path [:touch-trigger]
                           :localization-key "input-binding.touch-trigger"
                           :type :table
                           :columns [{:path [:input]
                                      :localization-key "input-binding.touch-trigger.input"
                                      :type :choicebox
                                      :options (sort-by first (make-options touch-values))
                                      :default (ffirst touch-values)}
                                     {:path [:action]
                                      :localization-key "input-binding.touch-trigger.action"
                                      :type :string}]}
                          {:path [:text-trigger]
                           :localization-key "input-binding.text-trigger"
                           :type :table
                           :columns [{:path [:input]
                                      :localization-key "input-binding.text-trigger.input"
                                      :type :choicebox
                                      :options (make-options text-values) ; Unsorted.
                                      :default (ffirst text-values)}
                                     {:path [:action]
                                      :localization-key "input-binding.text-trigger.action"
                                      :type :string}]}]}]}))

(defn- gamepad-pb->form-pb [pb]
  (letfn [(mods->bools [modlist]
            (let [mods (set (map :mod modlist))]
              {:negate (boolean (mods :gamepad-modifier-negate))
               :clamp (boolean (mods :gamepad-modifier-clamp))
               :scale (boolean (mods :gamepad-modifier-scale))}))
          (map->form-pb [m]
            (merge (dissoc m :mod) (mods->bools (m :mod))))
          (device->form-pb [device]
            (update device :map #(mapv map->form-pb %)))]
    (update pb :driver #(mapv device->form-pb %))))

(defn- form-pb->gamepad-pb [form-pb]
  (letfn [(bools->mods [{:keys [negate clamp scale]}]
            {:mod
             (vec
               (concat
                 (when negate [{:mod :gamepad-modifier-negate}])
                 (when clamp [{:mod :gamepad-modifier-clamp}])
                 (when scale [{:mod :gamepad-modifier-scale}])))})
          (f-map->map [f-map]
            (merge (dissoc f-map :negate :clamp :scale) (bools->mods f-map)))
          (f-device->device [f-device]
            (update f-device :map #(mapv f-map->map %)))]
    (update form-pb :driver #(mapv f-device->device %))))

(defn- gamepad-set-form-op [{:keys [node-id]} path value]
  (let [old-pb (g/node-value node-id :pb)
        old-form-pb (gamepad-pb->form-pb old-pb)
        upd-form-pb (assoc-in old-form-pb path value)]
    (g/set-property node-id :pb (form-pb->gamepad-pb upd-form-pb))))

(defmethod protobuf-form-data Input$GamepadMaps [node-id pb _def]
  (let [gamepad-values (butlast (protobuf/enum-values Input$Gamepad)) ; skip MAX_GAMEPAD_COUNT
        gamepad-type-values (protobuf/enum-values Input$GamepadType)]
    {:navigation false
     :form-ops {:user-data {:node-id node-id}
                :set gamepad-set-form-op}
     :sections [{:localization-key "gamepads"
                 :fields
                 [{:path [:driver]
                   :localization-key "gamepads.driver"
                   :type :2panel
                   :panel-key {:path [:device] :type :string}
                   :panel-form {:sections
                                [{:fields
                                  [{:path [:device]
                                    :localization-key "gamepads.driver.device"
                                    :type :string
                                    :default "New device"}
                                   {:path [:platform]
                                    :localization-key "gamepads.driver.platform"
                                    :type :string}
                                   {:path [:dead-zone]
                                    :localization-key "gamepads.driver.dead-zone"
                                    :type :number}
                                   {:path [:map]
                                    :localization-key "gamepads.driver.map"
                                    :type :table
                                    :columns [{:path [:input]
                                               :localization-key "gamepads.driver.map.input"
                                               :type :choicebox
                                               :options (sort-by first (make-options gamepad-values))
                                               :default (ffirst gamepad-values)}
                                              {:path [:type]
                                               :localization-key "gamepads.driver.map.type"
                                               :type :choicebox
                                               :options (sort-by first (make-options gamepad-type-values))
                                               :default (ffirst gamepad-type-values)}
                                              {:path [:index]
                                               :localization-key "gamepads.driver.map.index"
                                               :type :integer}
                                              {:path [:negate]
                                               :localization-key "gamepads.driver.map.negate"
                                               :type :boolean}
                                              {:path [:scale]
                                               :localization-key "gamepads.driver.map.scale"
                                               :type :boolean}
                                              {:path [:clamp]
                                               :localization-key "gamepads.driver.map.clamp"
                                               :type :boolean}
                                              {:path [:hat-mask]
                                               :localization-key "gamepads.driver.map.hat-mask"
                                               :type :integer}]}]}]}}]}]
     :values (make-values (gamepad-pb->form-pb pb) [:driver])}))

(def texture-profiles-unsupported-formats
  #{:texture-format-rgb16f
    :texture-format-rgb32f
    :texture-format-rgba16f
    :texture-format-rgba32f
    :texture-format-r16f
    :texture-format-rg16f
    :texture-format-r32f
    :texture-format-rg32f
    :texture-format-r-bc4
    :texture-format-rg-bc5
    :texture-format-rgb-bc1
    :texture-format-rgb-etc1
    :texture-format-rgba-pvrtc-2bppv1
    :texture-format-rgba-pvrtc-4bppv1
    :texture-format-rgb-pvrtc-2bppv1
    :texture-format-rgb-pvrtc-4bppv1
    :texture-format-rgba-bc3
    :texture-format-rgba-bc7
    :texture-format-rgba-etc2})

(defmethod protobuf-form-data Graphics$TextureProfiles [_node-id pb _def]
  (let [os-values (protobuf/enum-values Graphics$PlatformProfile$OS)
        format-values (distinct (protobuf/enum-values Graphics$TextureImage$TextureFormat))
        format-values-filtered (filterv
                                 (fn [fmt]
                                   (not (contains? texture-profiles-unsupported-formats (first fmt))))
                                 format-values)
        profile-options (mapv #(do [% %]) (map :name (:profiles pb)))
        ;; name + compressor instance
        available-compressors (mapv #(vector % (TextureCompression/getCompressor %)) (TextureCompression/getInstalledCompressorNames))]
    {:navigation false
     :sections
     [{:localization-key "texture-profiles"
       :fields
       [{:path [:path-settings]
         :localization-key "texture-profiles.path-settings"
         :type :table
         :columns [{:path [:path]
                    :localization-key "texture-profiles.path-settings.path"
                    :type :string
                    :default "**"}
                   {:path [:profile]
                    :localization-key "texture-profiles.path-settings.profile"
                    :type :choicebox
                    :from-string str
                    :to-string str                  ; allow manual entry
                    :options (sort-by first profile-options)
                    :default "Default"}]}
        {:path [:profiles]
         :localization-key "texture-profiles.profiles"
         :type :2panel
         :panel-key {:path [:name] :type :string :default "Default"}
         :panel-form {:sections
                      [{:fields
                        [{:path [:platforms]
                          :localization-key "texture-profiles.profiles.platforms"
                          :type :2panel
                          :panel-key {:path [:os]
                                      :type :choicebox
                                      :options (sort-by first (make-options os-values))
                                      :default (ffirst os-values)}
                          :panel-form {:sections
                                       [{:fields
                                         [{:path [:formats]
                                           :localization-key "texture-profiles.profiles.platforms.formats"
                                           :type :2panel
                                           :panel-key {:path [:format]
                                                       :localization-key "texture-profiles.profiles.platforms.formats.format"
                                                       :type :choicebox
                                                       :options (sort-by first (make-options format-values-filtered))
                                                       :default (ffirst format-values-filtered)}
                                           :panel-form-fn
                                           (fn panel-form-fn [selected-format]
                                             (let [texture-format (when (:format selected-format)
                                                                    (protobuf/val->pb-enum Graphics$TextureImage$TextureFormat (:format selected-format)))
                                                   available-compressors-for-format (keep (fn [[compressor-name compressor-instance]]
                                                                                            (when (.supportsTextureFormat ^ITextureCompressor compressor-instance texture-format)
                                                                                              [compressor-name compressor-name]))
                                                                                          available-compressors)
                                                   available-presets-for-compressor (mapv
                                                                                      (fn [preset-name]
                                                                                        [preset-name (.getDisplayName (TextureCompression/getPreset preset-name))])
                                                                                      (TextureCompression/getPresetNamesForCompressor (:compressor selected-format)))]
                                               {:sections
                                               [{:fields
                                                 [{:path [:compressor]
                                                    :localization-key "texture-profiles.profiles.platforms.formats.compressor"
                                                    :type :choicebox
                                                    :options (make-options available-compressors-for-format) ; Unsorted.
                                                    :default (TextureCompressorUncompressed/TextureCompressorName)}
                                                   {:path [:compressor-preset]
                                                    :localization-key "texture-profiles.profiles.platforms.formats.compressor-preset"
                                                    :type :choicebox
                                                    :options available-presets-for-compressor ; Unsorted.
                                                    :default (ffirst available-presets-for-compressor)}]}]}))}
                                          {:path [:mipmaps]
                                           :type :boolean
                                           :localization-key "texture-profiles.profiles.platforms.mipmaps"}
                                          {:path [:max-texture-size]
                                           :type :integer
                                           :localization-key "texture-profiles.profiles.platforms.max-texture-size"
                                           :default (protobuf/default Graphics$PlatformProfile :max-texture-size)
                                           :optional true}
                                          {:path [:premultiply-alpha]
                                           :type :boolean
                                           :localization-key "texture-profiles.profiles.platforms.premultiply-alpha"
                                           :default (protobuf/default Graphics$PlatformProfile :premultiply-alpha)
                                           :optional true}]}]}}]}]}}]}]}))

(defn produce-form-data
  ([node-id pb def]
   (produce-form-data node-id pb def (protobuf-form-data node-id pb def)))
  ([node-id pb _def form-data]
   (let [form-data (merge (default-form-ops node-id) form-data)]
     (if (contains? form-data :values)
       form-data
       (assoc form-data :values (form-values form-data pb))))))
