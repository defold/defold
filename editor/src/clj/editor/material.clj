(ns editor.material
  (:require [editor.protobuf :as protobuf]
            [editor.protobuf-forms :as protobuf-forms]
            [dynamo.graph :as g]
            [editor.graph-util :as gu]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.vertex :as vtx]
            [editor.gl.texture :as texture]
            [editor.project :as project]
            [editor.scene :as scene]
            [editor.workspace :as workspace]
            [editor.math :as math]
            [editor.resource :as resource]
            [editor.validation :as validation]
            [internal.render.pass :as pass])
  (:import [com.dynamo.render.proto Material$MaterialDesc Material$MaterialDesc$ConstantType Material$MaterialDesc$WrapMode Material$MaterialDesc$FilterModeMin Material$MaterialDesc$FilterModeMag]
           [editor.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [java.io PushbackReader]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Vector4d Matrix4d Point3d Quat4d]
           [editor.gl.shader ShaderLifecycle]))

(set! *warn-on-reflection* true)

(def pb-def {:ext "material"
             :icon "icons/32/Icons_31-Material.png"
             :pb-class Material$MaterialDesc
             :resource-fields [:vertex-program :fragment-program]
             :view-types [:form-view]
             :label "Material"})

(g/defnk produce-save-data [resource def pb]
  {:resource resource
   :content (protobuf/map->str (:pb-class def) pb)})

(defn- build-pb [self basis resource dep-resources user-data]
  (let [def (:def user-data)
        pb  (:pb user-data)
        pb  (if (:transform-fn def) ((:transform-fn def) pb) pb)
        pb  (reduce (fn [pb [label resource]]
                      (if (vector? label)
                        (assoc-in pb label resource)
                        (assoc pb label resource)))
                    pb (map (fn [[label res]]
                              [label (workspace/proj-path (get dep-resources res))])
                            (:dep-resources user-data)))]
    {:resource resource :content (protobuf/map->bytes (:pb-class user-data) pb)}))

(g/defnk produce-build-targets [_node-id project-id resource pb def dep-build-targets]
  (let [dep-build-targets (flatten dep-build-targets)
        deps-by-source (into {} (map #(let [res (:resource %)] [(workspace/proj-path (:resource res)) res]) dep-build-targets))
        resource-fields (mapcat (fn [field] (if (vector? field) (mapv (fn [i] (into [(first field) i] (rest field))) (range (count (get pb (first field))))) [field])) (:resource-fields def))
        dep-resources (map (fn [label] [label (get deps-by-source (if (vector? label) (get-in pb label) (get pb label)))]) resource-fields)]
    [{:node-id _node-id
      :resource (workspace/make-build-resource resource)
      :build-fn build-pb
      :user-data {:pb pb
                  :pb-class (:pb-class def)
                  :def def
                  :dep-resources dep-resources}
      :deps dep-build-targets}]))

(def ^:private form-data
  (let [constant-values (protobuf/enum-values Material$MaterialDesc$ConstantType)]
     {:sections
      [{:title "Material"
        :fields
        [{:path [:name]
          :label "Name"
          :type :string
          :default "New Material"}
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
                     :options (protobuf-forms/make-options constant-values)
                     :default (ffirst constant-values)}
                    {:path [:value] :label "Value" :type :vec4}]}
         {:path [:fragment-constants]
          :label "Fragment Constants"
          :type :table
          :columns [{:path [:name] :label "Name" :type :string}
                    {:path [:type]
                     :label "Type"
                     :type :choicebox
                     :options (protobuf-forms/make-options constant-values)
                     :default (ffirst constant-values)}
                    {:path [:value] :label "Value" :type :vec4}]}
         {:path [:samplers]
          :label "Samplers"
          :type :table
          :columns (let [wrap-options (protobuf/enum-values Material$MaterialDesc$WrapMode)
                         min-options (protobuf/enum-values Material$MaterialDesc$FilterModeMin)
                         mag-options (protobuf/enum-values Material$MaterialDesc$FilterModeMag)]
                     [{:path [:name] :label "Name" :type :string}
                      {:path [:wrap-u]
                       :label "Wrap U"
                       :type :choicebox
                       :options (protobuf-forms/make-options wrap-options)
                       :default (ffirst wrap-options)}
                      {:path [:wrap-v]
                       :label "Wrap V"
                       :type :choicebox
                       :options (protobuf-forms/make-options wrap-options)
                       :default (ffirst wrap-options)}
                      {:path [:filter-min]
                       :label "Filter Min"
                       :type :choicebox
                       :options (protobuf-forms/make-options min-options)
                       :default (ffirst min-options)}
                      {:path [:filter-mag]
                       :label "Filter Mag"
                       :type :choicebox
                       :options (protobuf-forms/make-options mag-options)
                       :default (ffirst mag-options)}])}
         {:path [:tags]
          :label "Tags"
          :type :list
          :element {:type :string :default "New Tag"}}]}]}))

(g/defnk produce-form-data [_node-id pb def vertex-program fragment-program]
  (let [pb (assoc pb
             :vertex-program (resource/resource->proj-path vertex-program)
             :fragment-program (resource/resource->proj-path fragment-program))]
    (protobuf-forms/produce-form-data _node-id pb def form-data)))

(defn- constant->val [constant]
  (case (:type constant)
    :constant-type-user (let [[x y z w] (:value constant)]
                          (Vector4d. x y z w))
    :constant-type-viewproj :view-proj
    :constant-type-world :world
    :constant-type-texture :texture
    :constant-type-view :view
    :constant-type-projection :projection
    :constant-type-normal :normal
    :constant-type-worldview :world-view))

(def ^:private wrap-mode->gl {:wrap-mode-repeat GL2/GL_REPEAT
                              :wrap-mode-mirrored-repeat GL2/GL_MIRRORED_REPEAT
                              :wrap-mode-clamp-to-edge GL2/GL_CLAMP_TO_EDGE})

(def ^:private filter-mode-min->gl {:filter-mode-min-nearest GL2/GL_NEAREST
                                    :filter-mode-min-linear GL2/GL_LINEAR
                                    :filter-mode-min-nearest-mipmap-nearest GL2/GL_NEAREST_MIPMAP_NEAREST
                                    :filter-mode-min-nearest-mipmap-linear GL2/GL_NEAREST_MIPMAP_LINEAR
                                    :filter-mode-min-linear-mipmap-nearest GL2/GL_LINEAR_MIPMAP_NEAREST
                                    :filter-mode-min-linear-mipmap-linear GL2/GL_LINEAR_MIPMAP_LINEAR})

(def ^:private filter-mode-mag->gl {:filter-mode-mag-nearest GL2/GL_NEAREST
                                    :filter-mode-mag-linear GL2/GL_LINEAR})

(def ^:private default-sampler {:wrap-u :wrap-mode-clamp-to-edge
                                :wrap-v :wrap-mode-clamp-to-edge
                                :filter-min :filter-mode-min-linear
                                :filter-mag :filter-mode-mag-linear})

(defn sampler->tex-params [sampler]
  (let [s (or sampler default-sampler)]
    {:wrap-s (wrap-mode->gl (:wrap-u s))
     :wrap-t (wrap-mode->gl (:wrap-v s))
     :min-filter (filter-mode-min->gl (:filter-min s))
     :mag-filter (filter-mode-mag->gl (:filter-mag s))}))

(g/defnode MaterialNode
  (inherits project/ResourceNode)

  (property pb g/Any (dynamic visible (g/always false)))
  (property def g/Any (dynamic visible (g/always false)))
  (property vertex-program (g/protocol resource/Resource)
    (dynamic visible (g/always false))
    (value (gu/passthrough vertex-resource))
    (set (project/gen-resource-setter [[:resource :vertex-resource]
                                       [:full-source :vertex-source]]))
    (validate (validation/validate-resource vertex-program)))

  (property fragment-program (g/protocol resource/Resource)
    (dynamic visible (g/always false))
    (value (gu/passthrough fragment-resource))
    (set (project/gen-resource-setter [[:resource :fragment-resource]
                                       [:full-source :fragment-source]]))
    (validate (validation/validate-resource fragment-program)))

  (output form-data g/Any :cached produce-form-data)

  (input dep-build-targets g/Any :array)
  (input vertex-resource (g/protocol resource/Resource))
  (input vertex-source g/Str)
  (input fragment-resource (g/protocol resource/Resource))
  (input fragment-source g/Str)

  (output save-data g/Any :cached produce-save-data)
  (output build-targets g/Any :cached produce-build-targets)
  (output scene g/Any (g/always {}))
  (output shader ShaderLifecycle :cached (g/fnk [_node-id vertex-source fragment-source pb]
                                           (let [uniforms (into {} (map (fn [constant] [(:name constant) (constant->val constant)]) (concat (:vertex-constants pb) (:fragment-constants pb))))]
                                             (shader/make-shader _node-id vertex-source fragment-source uniforms))))
  (output samplers [{g/Keyword g/Any}] :cached (g/fnk [pb] (vec (:samplers pb)))))

(defn- connect-build-targets [project self resource path]
  (let [resource (workspace/resolve-resource resource path)]
    (project/connect-resource-node project resource self [[:build-targets :dep-build-targets]])))

(defn load-material [project self resource]
  (let [def pb-def
        pb (protobuf/read-text (:pb-class def) resource)
        pb (-> pb
             (update :samplers into
                     (mapv (fn [tex] (assoc default-sampler :name tex))
                       (:textures pb)))
             (dissoc :textures))]
    (concat
      (g/set-property self :pb pb)
      (g/set-property self :def def)
      (g/set-property self :vertex-program (workspace/resolve-resource resource (:vertex-program pb)))
      (g/set-property self :fragment-program (workspace/resolve-resource resource (:fragment-program pb)))
      (for [res (:resource-fields def)]
        (if (vector? res)
          (for [v (get pb (first res))]
            (let [path (if (second res) (get v (second res)) v)]
              (connect-build-targets project self resource path)))
          (connect-build-targets project self resource (get pb res)))))))

(defn- register [workspace def]
  (workspace/register-resource-type workspace
                                 :ext (:ext def)
                                 :label (:label def)
                                 :build-ext (:build-ext def)
                                 :node-type MaterialNode
                                 :load-fn load-material
                                 :icon (:icon def)
                                 :view-types (:view-types def)
                                 :tags (:tags def)
                                 :template (:template def)))

(defn register-resource-types [workspace]
  (register workspace pb-def))
