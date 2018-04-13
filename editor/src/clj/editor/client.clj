(ns editor.client
  (:require [editor.protobuf :as protobuf]
            [editor.prefs :as prefs]
            [service.log :as log])
  (:import [clojure.lang ExceptionInfo]
           [com.defold.editor.client DefoldAuthFilter]
           [com.defold.editor.providers ProtobufProviders ProtobufProviders$ProtobufMessageBodyReader ProtobufProviders$ProtobufMessageBodyWriter]
           [com.dynamo.cr.protocol.proto Protocol$UserInfo Protocol$NewProject Protocol$ProjectInfo]
           [com.google.protobuf Message]
           [com.sun.jersey.api.client Client ClientHandlerException ClientResponse WebResource WebResource$Builder UniformInterfaceException]
           [com.sun.jersey.api.client.config ClientConfig DefaultClientConfig]
           [java.io InputStream ByteArrayInputStream]
           [java.net URI]
           [javax.ws.rs.core MediaType]))

(set! *warn-on-reflection* true)

(defn- make-client-config []
  (let [cc (DefaultClientConfig.)]
    (.add (.getClasses cc) ProtobufProviders$ProtobufMessageBodyReader)
    (.add (.getClasses cc) ProtobufProviders$ProtobufMessageBodyWriter)
    cc))

(defn make-client [prefs]
  (let [client (Client/create (make-client-config))
        email (prefs/get-prefs prefs "email" nil)
        token (prefs/get-prefs prefs "token" nil)]
    (when (and email token)
      (.addFilter client (DefoldAuthFilter. email token nil)))
    {:client client
     :prefs prefs}))

(def ^"[Ljavax.ws.rs.core.MediaType;" ^:private
  media-types
  (into-array MediaType [ProtobufProviders/APPLICATION_XPROTOBUF_TYPE]))

(defn- server-url
  ^URI [prefs]
  (let [server-url (URI. (prefs/get-prefs prefs "server-url" "https://cr.defold.com"))]
    (when-not (= "https" (.getScheme server-url))
      (throw (ex-info (format "Invalid 'server-url' preference: must use https scheme: was '%s'" server-url)
                      {:server-url server-url})))
    server-url))

(defn- cr-resource ^WebResource$Builder [client paths ^"[Ljavax.ws.rs.core.MediaType;" accept-types]
  (let [{:keys [^Client client prefs]} client
        ^WebResource resource (reduce (fn [^WebResource resource path] (.path resource (str path))) (.resource client (server-url prefs)) paths)]
    (if (seq accept-types)
      (.accept resource accept-types)
      (.getRequestBuilder resource))))

(defn cr-get [client paths ^Class pb-resp-class]
  (let [builder (cr-resource client paths media-types)]
    (protobuf/pb->map (.get builder pb-resp-class))))

(defn- resp->clj [^ClientResponse resp]
  {:status (.getStatus resp)
   :message (.toString resp)})

(defn- cr-post [client paths ^InputStream stream ^MediaType type ^Class pb-resp-class]
  (let [accept-types (if pb-resp-class media-types [])
        ^WebResource$Builder builder (-> client
                                       (cr-resource paths accept-types)
                                       (.type type))]
    (if pb-resp-class
      (try
        (-> builder
          (.post pb-resp-class stream)
          (protobuf/pb->map))
        (catch UniformInterfaceException e
          (throw (ex-info (.getMessage e) (resp->clj (.getResponse e)))))
        (catch ClientHandlerException e
          (let [msg (.getMessage e)]
            (throw (ex-info msg {:message msg})))))
      (with-open [^ClientResponse resp (-> builder
                                         (.post ClientResponse stream))]
        (let [resp (resp->clj resp)]
          (when (not= 200 (:status resp))
            (throw (ex-info (:message resp) resp))))))))

(defn- cr-post-pb [client paths ^Class pb-class pb-map ^Class pb-resp-class]
  (let [stream (ByteArrayInputStream. (protobuf/map->bytes pb-class pb-map))]
    (cr-post client paths stream ProtobufProviders/APPLICATION_XPROTOBUF_TYPE pb-resp-class)))

(defn user-info [client]
  (when-let [email (prefs/get-prefs (:prefs client) "email" nil)]
    (cr-get client ["users" email] Protocol$UserInfo)))

(defn upload-engine [client user-id cr-project-id ^String platform ^InputStream stream]
  (try
    (cr-post client ["projects" user-id cr-project-id "engine" platform] [] stream MediaType/APPLICATION_OCTET_STREAM_TYPE nil)
    (catch ExceptionInfo e
      (let [resp (ex-data e)]
        (throw (ex-info (format "Could not upload engine %d: %s" (:status resp) (:message resp)) resp))))))

(defn new-project [client user-id new-project-pb-map]
  (try
    (cr-post-pb client ["projects" user-id] Protocol$NewProject new-project-pb-map Protocol$ProjectInfo)
    (catch ExceptionInfo e
      (let [resp (ex-data e)]
        (throw (ex-info (format "Could not create new project %d: %s" (:status resp) (:message resp)) resp))))))
