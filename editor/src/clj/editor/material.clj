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
            [editor.defold-project :as project]
            [editor.scene :as scene]
            [editor.workspace :as workspace]
            [editor.math :as math]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.validation :as validation]
            [editor.gl.pass :as pass]
            [internal.util :as util])
  (:import [com.dynamo.render.proto Material$MaterialDesc Material$MaterialDesc$ConstantType Material$MaterialDesc$WrapMode Material$MaterialDesc$FilterModeMin Material$MaterialDesc$FilterModeMag]
           [editor.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [java.io PushbackReader]
           [com.jogamp.opengl GL GL2 GLContext GLDrawableFactory]
           [com.jogamp.opengl.glu GLU]
           [javax.vecmath Vector4d Matrix4d Point3d Quat4d]
           [editor.gl.shader ShaderLifecycle]))

(set! *warn-on-reflection* true)

(g/defnk produce-pb-msg [name vertex-program fragment-program vertex-constants fragment-constants samplers tags :as pb-msg]
  (-> pb-msg
      (dissoc :_node-id :basis)
      (update :vertex-program resource/resource->proj-path)
      (update :fragment-program resource/resource->proj-path)))

(defn- build-material [resource dep-resources user-data]
  (let [pb (reduce (fn [pb [label resource]] (assoc pb label resource))
                   (:pb-msg user-data)
                   (map (fn [[label res]] [label (resource/proj-path (get dep-resources res))]) (:dep-resources user-data)))]
    {:resource resource :content (protobuf/map->bytes Material$MaterialDesc pb)}))

(defn- prop-resource-error [_node-id prop-kw prop-value prop-name resource-ext]
  (validation/prop-error :fatal _node-id prop-kw validation/prop-resource-ext? prop-value resource-ext prop-name))

(g/defnk produce-build-targets [_node-id resource pb-msg dep-build-targets vertex-program fragment-program]
  (or (prop-resource-error _node-id :vertex-program vertex-program "Vertex Program" "vp")
      (prop-resource-error _node-id :fragment-program fragment-program "Fragment Program" "fp")
      (let [dep-build-targets (flatten dep-build-targets)
            deps-by-source (into {} (map #(let [res (:resource %)] [(:resource res) res]) dep-build-targets))
            dep-resources (map (fn [[label resource]] [label (get deps-by-source resource)])
                               [[:vertex-program vertex-program]
                                [:fragment-program fragment-program]])]
        [{:node-id _node-id
          :resource (workspace/make-build-resource resource)
          :build-fn build-material
          :user-data {:pb-msg pb-msg
                      :dep-resources dep-resources}
          :deps dep-build-targets}])))

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

(defn- set-form-op [{:keys [node-id]} [property] value]
  (g/set-property! node-id property value))

(defn- clear-form-op [{:keys [node-id]} [property]]
  (g/clear-property! node-id property))

(g/defnk produce-form-data [_node-id name vertex-program fragment-program vertex-constants fragment-constants samplers tags :as args]
  (let [values (-> (select-keys args (mapcat :path (get-in form-data [:sections 0 :fields]))))
        form-values (into {} (map (fn [[k v]] [[k] v]) values))]
    (-> form-data
        (assoc :values form-values)
        (assoc :form-ops {:user-data {:node-id _node-id}
                          :set set-form-op
                          :clear clear-form-op}))))

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
  (inherits resource-node/ResourceNode)

  (property name g/Str (dynamic visible (g/constantly false)))

  (property vertex-program resource/Resource
    (dynamic visible (g/constantly false))
    (value (gu/passthrough vertex-resource))
    (set (fn [_evaluation-context self old-value new-value]
           (project/resource-setter self old-value new-value
                                    [:resource :vertex-resource]
                                    [:full-source :vertex-source]
                                    [:build-targets :dep-build-targets]))))

  (property fragment-program resource/Resource
    (dynamic visible (g/constantly false))
    (value (gu/passthrough fragment-resource))
    (set (fn [_evaluation-context self old-value new-value]
           (project/resource-setter self old-value new-value
                                    [:resource :fragment-resource]
                                    [:full-source :fragment-source]
                                    [:build-targets :dep-build-targets]))))

  (property vertex-constants g/Any (dynamic visible (g/constantly false)))
  (property fragment-constants g/Any (dynamic visible (g/constantly false)))
  (property samplers g/Any (dynamic visible (g/constantly false)))
  (property tags g/Any (dynamic visible (g/constantly false)))

  (output form-data g/Any :cached produce-form-data)

  (input dep-build-targets g/Any :array)
  (input vertex-resource resource/Resource)
  (input vertex-source g/Str)
  (input fragment-resource resource/Resource)
  (input fragment-source g/Str)

  (output pb-msg g/Any :cached produce-pb-msg)

  (output save-value g/Any (gu/passthrough pb-msg))
  (output build-targets g/Any :cached produce-build-targets)
  (output scene g/Any (g/constantly {}))
  (output shader ShaderLifecycle :cached (g/fnk [_node-id vertex-source vertex-program fragment-source fragment-program vertex-constants fragment-constants]
                                                (or (prop-resource-error _node-id :vertex-program vertex-program "Vertex Program" "vp")
                                                    (prop-resource-error _node-id :fragment-program fragment-program "Fragment Program" "fp")
                                                    (let [uniforms (into {} (map (fn [constant] [(:name constant) (constant->val constant)])
                                                                                 (concat vertex-constants fragment-constants)))]
                                                      (shader/make-shader _node-id vertex-source fragment-source uniforms)))))
  (output samplers [g/KeywordMap] :cached (g/fnk [samplers] (vec samplers))))

(defn- make-sampler [name]
  (assoc default-sampler :name name))

(defn load-material [project self resource pb]
  (concat
    (g/set-property self :vertex-program (workspace/resolve-resource resource (:vertex-program pb)))
    (g/set-property self :fragment-program (workspace/resolve-resource resource (:fragment-program pb)))
    (for [field [:name :vertex-constants :fragment-constants :samplers :tags]]
      (g/set-property self field (field pb)))))

(defn- convert-textures-to-samplers
  "The old format specified :textures as string names. Convert these into
  :samplers if we encounter them. Ignores :textures that already have
  :samplers with the same name. Also ensures that there are no duplicate
  entries in the :samplers list, based on :name."
  [pb]
  (let [existing-samplers (:samplers pb)
        samplers-created-from-textures (map make-sampler (:textures pb))
        samplers (into [] (util/distinct-by :name) (concat existing-samplers samplers-created-from-textures))]
    (-> pb
        (assoc :samplers samplers)
        (dissoc :textures))))

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
    :ext "material"
    :label "Material"
    :node-type MaterialNode
    :ddf-type Material$MaterialDesc
    :load-fn load-material
    :sanitize-fn convert-textures-to-samplers
    :icon "icons/32/Icons_31-Material.png"
    :view-types [:form-view :text]))
