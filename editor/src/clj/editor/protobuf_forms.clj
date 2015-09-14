(ns editor.protobuf-forms
  (:require [editor.protobuf :as protobuf]
            [clojure.string :as str]
            [dynamo.graph :as g])
  (:import [com.dynamo.input.proto Input$InputBinding Input$Key Input$Mouse Input$GamepadMaps Input$Gamepad Input$GamepadType Input$Touch Input$Text]
           [com.dynamo.render.proto Render$RenderPrototypeDesc Material$MaterialDesc Material$MaterialDesc$ConstantType]
           [com.dynamo.render.proto Render$DisplayProfiles]
           [com.dynamo.graphics.proto Graphics$TextureProfiles Graphics$PlatformProfile$OS Graphics$TextureFormatAlternative$CompressionLevel Graphics$TextureImage$TextureFormat]))

(defn- set-form-op [{:keys [node-id]} path value]
  (g/update-property! node-id :pb assoc-in path value))

(defn- display-name-or-default [[val {:keys [display-name]}]]
  (if (str/blank? display-name)
    (name val)
    display-name))

(defn- make-options [enum-values]
  (map (juxt first display-name-or-default) enum-values))

(defn- default-form-ops [node-id]
  {:form-ops {:user-data {:node-id node-id}
              :set set-form-op
              :clear nil}})

(defmulti protobuf-form-data (fn [node-id pb def] (:pb-class def)))

(defmethod protobuf-form-data Material$MaterialDesc [node-id pb def]
  (let [constant-values (protobuf/enum-values Material$MaterialDesc$ConstantType)]
    {
     :sections
     [
      {
       :title "Material"
       :fields
       [
        {:path [:name]
         :label "Name"
         :type :string
         :default "cakes"}
        {:path [:vertex-program]
         :label "Vertex Program"
         :type :resource :filter "vp"}
        {:path [:fragment-program]
         :label "Fragment Program"
         :type :resource :filter "fp"}
        {:path [:vertex-constants]
         :label "Vertex Constants"
         :type :table
         :columns [{:path [:name] :label "Name" :type :string}
                   {:path [:type]
                    :label "Type"
                    :type :choicebox
                    :options (make-options constant-values)
                    :default (ffirst constant-values)
                    }
                   {:path [:value] :label "Value" :type :vec4}]
         }
        {:path [:tags]
         :label "Tags"
         :type :list
         :element {:type :string :default "new tag"}
         }
        ]
       }
      ]
     :values
     {[:name] {:value (get pb :name) :source :explicit}
      [:vertex-program] {:value (get pb :vertex-program) :source :explicit}
      [:fragment-program] {:value (get pb :fragment-program) :source :explicit}
      [:tags] {:value (get pb :tags) :source :explicit}
      [:vertex-constants] {:value (get pb :vertex-constants) :source :explicit}
      }
     }))

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
     :values
     {
      [:key-trigger] {:value (get pb :key-trigger) :source :explicit}
      [:mouse-trigger] {:value (get pb :mouse-trigger) :source :explicit}
      [:gamepad-trigger] {:value (get pb :gamepad-trigger) :source :explicit}
      [:touch-trigger] {:value (get pb :touch-trigger) :source :explicit}
      [:text-trigger] {:value (get pb :text-trigger) :source :explicit}
      }
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
     :values
     {
      [:driver] {:value ((gamepad-pb->form-pb pb) :driver) :source :explicit}
      }
     }))

(defmethod protobuf-form-data Render$DisplayProfiles [node-id pb def]
  {
   :sections
   [
    {
     :title "Display Profiles"
     :fields
     [
      {
       :path [:profiles]
       :label "Profile"
       :type :2panel
       :panel-key {:path [:name] :type :string}
       :panel-form
       {
        :sections
        [
         {
          :fields
          [
           {
            :path [:name]
            :label "Name"
            :type :string
            :default "New Display Profile"
            }
           {
            :path [:qualifiers]
            :label "Qualifiers"
            :type :table
            :columns
            [
             {
              :path [:width]
              :label "Width"
              :type :integer
              }
             {
              :path [:height]
              :label "Height"
              :type :integer
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
   :values
   {
    [:profiles] {:value (pb :profiles) :source :explicit}
    }
   })

(defmethod protobuf-form-data Graphics$TextureProfiles [node-id pb def]
  (let [os-values (protobuf/enum-values Graphics$PlatformProfile$OS)
        format-values (protobuf/enum-values Graphics$TextureImage$TextureFormat)
        compression-values (protobuf/enum-values Graphics$TextureFormatAlternative$CompressionLevel)
        ]
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
               :type :string
               :default "Default"
               }
              ]
             }
            {
             :path [:profiles]
             :label "Profiles"
             :type :2panel
             :panel-key {:path [:name] :type :string}
             :panel-form
             {
              :sections
              [
               {
                :fields
                [
                 {
                  :path [:name]
                  :label "Name"
                  :type :string
                  :default "New Profile"
                  }
                 {
                  :path [:platforms]
                  :label "Platforms"
                  :type :2panel
                  :panel-key
                  {:path [:os] :type :choicebox :options (make-options os-values)}
                  :panel-form
                  {
                   :sections
                   [
                    {
                     :fields
                     [
                      {
                       :path [:os]
                       :label "OS"
                       :type :choicebox
                       :options (make-options os-values)
                       :default (ffirst os-values)
                       }
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
                       ;;todo actually optional
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
         :values
         {
          [:path-settings] {:value (pb :path-settings) :source :explicit}
          [:profiles] {:value (pb :profiles) :source :explicit}
          }
         }))

(defmethod protobuf-form-data Render$RenderPrototypeDesc [node-id pb def]
  {
   :sections
   [
    {
     :title "Render"
     :fields
     [
      {
       :path [:script]
       :type :resource
       :filter "render_script"
       :label "Script"
       }
      {
       :path [:materials]
       :type :table
       :label "Materials"
       :columns [{:path [:name] :label "Name" :type :string :default "New Material"}
                 {:path [:material] :label "Material" :type :resource :filter "material" :default ""}]
       }
      ]
     }
    ]
   :values
   {
    [:script] {:value (pb :script) :source :explicit}
    [:materials] {:value (pb :materials) :source :explicit}
    }
   })

(defn produce-form-data [node-id pb def]
  (merge (default-form-ops node-id)
         (protobuf-form-data node-id pb def)))




