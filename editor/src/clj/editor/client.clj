(ns editor.client
  (:require [editor.protobuf :as protobuf]
            [editor.prefs :as prefs])
  (:import [com.defold.editor.client DefoldAuthFilter]
           [com.defold.editor.providers ProtobufProviders ProtobufProviders$ProtobufMessageBodyReader ProtobufProviders$ProtobufMessageBodyWriter]
           [com.sun.jersey.api.client Client ClientResponse WebResource WebResource$Builder]
           [com.sun.jersey.api.client.config ClientConfig DefaultClientConfig]
           [com.sun.jersey.multipart FormDataMultiPart]
           [com.sun.jersey.multipart.file FileDataBodyPart StreamDataBodyPart]
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

; NOTE: Version without exceptions. Haven't decided yet...
#_(defn rget [client path entity-class]
   (let [server-url (prefs/get-prefs (:prefs client) "server-url" "http://cr.defold.com")
         resource (.resource (:client client) (URI. server-url))]
     (let [^ClientResponse cr (-> resource
                                (.path path)
                                (.accept (into-array MediaType [ProtobufProviders/APPLICATION_XPROTOBUF_TYPE]))
                                (.get ClientResponse))]
       { :status (.getStatus cr)
         :entity (when (< (.getStatus cr) 300) (.getEntity cr entity-class))})))

(def ^"[Ljavax.ws.rs.core.MediaType;" ^:private
  media-types
  (into-array MediaType [ProtobufProviders/APPLICATION_XPROTOBUF_TYPE]))

; Protobuf version as json
(defn rget [client ^String path ^Class entity-class]
  (let [server-url (prefs/get-prefs (:prefs client) "server-url" "http://cr.defold.com")
        resource   (.resource ^Client (:client client) (URI. server-url))
        builder    (.accept (.path resource path) media-types)]
    (protobuf/pb->map (.get builder entity-class))))

(defn rpost [client ^String path]
  (let [server-url "http://localhost:9000"
        resource   (.resource ^Client (:client client) (URI. server-url))
        builder    (.accept (.path resource path) media-types)
        form (FormDataMultiPart.  )]
    (.bodyPart form (FileDataBodyPart. "file" (clojure.java.io/file "/tmp/foo.cpp") MediaType/APPLICATION_OCTET_STREAM_TYPE))
    (let [cr (.post (.type builder MediaType/MULTIPART_FORM_DATA_TYPE) ClientResponse form)]
      (count (slurp (.getEntityInputStream cr))))))

(let [c (make-client (prefs/make-prefs "defold"))]
  (rpost c "/build"))

