(ns editor.protobuf-forms
  (:require [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [clojure.string :as str]
            [dynamo.graph :as g])
  (:import [com.dynamo.input.proto Input$InputBinding Input$Key Input$Mouse Input$GamepadMaps Input$Gamepad Input$GamepadType Input$Touch Input$Text]
           [com.dynamo.graphics.proto Graphics$TextureProfiles Graphics$PlatformProfile$OS Graphics$TextureFormatAlternative$CompressionLevel Graphics$TextureImage$TextureFormat Graphics$TextureImage$CompressionType]))

(set! *warn-on-reflection* true)

(defn- dissoc-in
  "Dissociates an entry from a nested associative structure returning a new
  nested structure. keys is a sequence of keys. Any empty maps that result
  will not be present in the new structure."
  [m [k & ks :as keys]]
  (if ks
    (if-let [nextmap (get m k)]
      (let [newmap (dissoc-in nextmap ks)]
        (if (empty? newmap)
          (dissoc m k)
          (assoc m k newmap)))
      m)
    (dissoc m k)))

(defn- clear-form-op [{:keys [node-id]} path]
  (g/update-property! node-id :pb dissoc-in path))

(defn- set-form-op [{:keys [node-id]} path value]
  (g/update-property! node-id :pb assoc-in path value))

(defn- display-name-or-default [[val {:keys [display-name]}]]
  (if (str/blank? display-name)
    (name val)
    display-name))

(defn make-options [enum-values]
  (map (juxt first display-name-or-default) enum-values))

(defn- default-form-ops [node-id]
  {:form-ops {:user-data {:node-id node-id}
              :set set-form-op
              :clear clear-form-op}})

(defn- make-values [pb keys]
  (into {} (map #(do [[%] (pb %)]) keys)))

(defn- form-values [form-data pb]
  (let [keys (map (comp first :path) (mapcat :fields (:sections form-data)))]
    (make-values pb keys)))

(defmulti protobuf-form-data (fn [node-id pb def] (:pb-class def)))

(defmethod protobuf-form-data Input$InputBinding [node-id pb def]
  (let [key-values (butlast (protobuf/enum-values Input$Key)) ; skip MAX_KEY_COUNT
        mouse-values (butlast (protobuf/enum-values Input$Mouse)) ; skip MAX_MOUSE_COUNT
        gamepad-values (butlast (protobuf/enum-values Input$Gamepad)) ; skip MAX_GAMEPAD_COUNT
        touch-values (butlast (protobuf/enum-values Input$Touch)) ; skip MAX_TOUCH_COUNT
        text-values (butlast (protobuf/enum-values Input$Text)) ; skip MAX_TEXT_COUNT
        ]
    {
     :sections
     [
      {
       :title "Input Bindings"
       :fields
       [
        {
         :path [:key-trigger]
         :label "Key Triggers"
         :type :table
         :columns [{:path [:input]
                    :label "Input"
                    :type :choicebox
                    :options (make-options key-values)
                    :default (ffirst key-values)
                    }
                   {:path [:action] :label "Action" :type :string}
                   ]
         }
        {
         :path [:mouse-trigger]
         :label "Mouse Triggers"
         :type :table
         :columns [{:path [:input]
                    :label "Input"
                    :type :choicebox
                    :options (make-options mouse-values)
                    :default (ffirst mouse-values)
                    }
                   {:path [:action] :label "Action" :type :string}
                   ]
         }
        {
         :path [:gamepad-trigger]
         :label "Gamepad Triggers"
         :type :table
         :columns [{:path [:input]
                    :label "Input"
                    :type :choicebox
                    :options (make-options gamepad-values)
                    :default (ffirst gamepad-values)
                    }
                   {:path [:action] :label "Action" :type :string}
                   ]
         }
        {
         :path [:touch-trigger]
         :label "Touch Triggers"
         :type :table
         :columns [{:path [:input]
                    :label "Input"
                    :type :choicebox
                    :options (make-options touch-values)
                    :default (ffirst touch-values)
                    }
                   {:path [:action] :label "Action" :type :string}
                   ]
         }
        {
         :path [:text-trigger]
         :label "Text Triggers"
         :type :table
         :columns [{:path [:input]
                    :label "Input"
                    :type :choicebox
                    :options (make-options text-values)
                    :default (ffirst text-values)
                    }
                   {:path [:action] :label "Action" :type :string}
                   ]
         }
       ]
       }
      ]
     }))

(defn- gamepad-pb->form-pb [pb]
  (letfn [(mods->bools [modlist]
            (let [mods (set (map :mod modlist))]
              {
               :negate (boolean (mods :gamepad-modifier-negate))
               :clamp (boolean (mods :gamepad-modifier-clamp))
               :scale (boolean (mods :gamepad-modifier-scale))
               }))
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
            (update f-device :map #(mapv f-map->map %)))
          ]
    (update form-pb :driver #(mapv f-device->device %))))

(defn- gamepad-set-form-op [{:keys [node-id]} path value]
  (let [old-pb (g/node-value node-id :pb)
        old-form-pb (gamepad-pb->form-pb old-pb)
        upd-form-pb (assoc-in old-form-pb path value)]
    (g/set-property! node-id :pb (form-pb->gamepad-pb upd-form-pb))))

(defmethod protobuf-form-data Input$GamepadMaps [node-id pb def]
  (let [gamepad-values (butlast (protobuf/enum-values Input$Gamepad)) ; skip MAX_GAMEPAD_COUNT
        gamepad-type-values (protobuf/enum-values Input$GamepadType)]
    {
     :form-ops {:user-data {:node-id node-id}
                :set gamepad-set-form-op
                }
     :sections
     [
      {
       :title "Gamepads"
       :fields
       [
        {
         :path [:driver]
         :label "Gamepad"
         :type :2panel
         :panel-key {:path [:device] :type :string}
         :panel-form
         {
          :sections
          [
           {
            :fields
            [
             {:path [:device]
              :label "Device"
              :type :string
              :default "New device"
              }
             {:path [:platform]
              :label "Platform"
              :type :string
              }
             {:path [:dead-zone]
              :label "Dead zone"
              :type :number
              }
             {:path [:map]
              :label "Map"
              :type :table
              :columns [{:path [:input]
                         :label "Input"
                         :type :choicebox
                         :options (make-options gamepad-values)
                         :default (ffirst gamepad-values)
                         }
                        {:path [:type]
                         :label "Type"
                         :type :choicebox
                         :options (make-options gamepad-type-values)
                         :default (ffirst gamepad-type-values)
                         }
                        {:path [:index]
                         :label "Index"
                         :type :integer
                         }
                        {
                         :path [:negate]
                         :label "Negate"
                         :type :boolean
                         }
                        {
                         :path [:scale]
                         :label "Scale"
                         :type :boolean
                         }
                        {
                         :path [:clamp]
                         :label "Clamp"
                         :type :boolean
                         }
                        ]
              }
             ]
            }
           ]
          }
         }
        ]
       }
      ]
     :values (make-values (gamepad-pb->form-pb pb) [:driver])
     }))

(defmethod protobuf-form-data Graphics$TextureProfiles [node-id pb def]
  (let [os-values (protobuf/enum-values Graphics$PlatformProfile$OS)
        format-values (protobuf/enum-values Graphics$TextureImage$TextureFormat)
        compression-values (protobuf/enum-values Graphics$TextureFormatAlternative$CompressionLevel)
        compression-types (protobuf/enum-values Graphics$TextureImage$CompressionType)
        profile-options (mapv #(do [% %]) (map :name (:profiles pb)))]
        {
         :sections
         [
          {
           :title "Texture Profiles"
           :fields
           [
            {
             :path [:path-settings]
             :label "Path Settings"
             :type :table
             :columns
             [
              {
               :path [:path]
               :label "Path"
               :type :string
               :default "**"
               }
              {
               :path [:profile]
               :label "Profile"
               :type :choicebox
               :from-string str :to-string str ; allow manual entry
               :options profile-options
               :default "Default"
               }
              ]
             }
            {
             :path [:profiles]
             :label "Profiles"
             :type :2panel
             :panel-key {:path [:name] :type :string :default "Default"}
             :panel-form
             {
              :sections
              [
               {
                :fields
                [
                 {
                  :path [:platforms]
                  :label "Platforms"
                  :type :2panel
                  :panel-key
                  {:path [:os] :type :choicebox :options (make-options os-values) :default (ffirst os-values)}
                  :panel-form
                  {
                   :sections
                   [
                    {
                     :fields
                     [
                      {
                       :path [:formats]
                       :label "Formats"
                       :type :table
                       :columns
                       [
                        {:path [:format]
                         :label "Format"
                         :type :choicebox
                         :options (make-options format-values)
                         :default (ffirst format-values)
                         }
                        {
                         :path [:compression-level]
                         :label "Compression"
                         :type :choicebox
                         :options (make-options compression-values)
                         :default (ffirst compression-values)
                         }
                        {
                         :path [:compression-type]
                         :label "Type"
                         :type :choicebox
                         :options (make-options compression-types)
                         :default (ffirst compression-types)
                         }
                        ]
                       }
                      {
                       :path [:mipmaps]
                       :type :boolean
                       :label "Mipmaps"
                       }
                      {
                       :path [:max-texture-size]
                       :type :integer
                       :label "Max texture size"
                       :default 0
                       :optional true
                       }
                      ]
                     }
                    ]
                   }
                  }
                 ]
                }
               ]
              }
             }
            ]
           }
          ]
         }))

(defn produce-form-data
  ([node-id pb def]
    (produce-form-data node-id pb def (protobuf-form-data node-id pb def)))
  ([node-id pb def form-data]
    (let [form-data (merge (default-form-ops node-id) form-data)]
      (if (contains? form-data :values)
        form-data
        (assoc form-data :values (form-values form-data pb))))))
