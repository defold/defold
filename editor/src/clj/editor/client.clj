(ns editor.client
  (:require [dynamo.file.protobuf :as protobuf]
            [editor.prefs :as prefs])
  (:import [java.net URI]
           [javax.ws.rs.core MediaType]
           [com.sun.jersey.api.client Client ClientResponse WebResource]
           [com.sun.jersey.api.client.config ClientConfig DefaultClientConfig]
           [com.defold.editor.client DefoldAuthFilter]
           [com.defold.editor.providers ProtobufProviders ProtobufProviders$ProtobufMessageBodyReader ProtobufProviders$ProtobufMessageBodyWriter]))

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

; Protobuf version as json
(defn rget [client path entity-class]
  (let [server-url (prefs/get-prefs (:prefs client) "server-url" "http://cr.defold.com")
        resource (.resource (:client client) (URI. server-url))]
    (-> resource
      (.path path)
      (.accept (into-array MediaType [ProtobufProviders/APPLICATION_XPROTOBUF_TYPE]))
      (.get entity-class)
      (protobuf/pb->map))))

