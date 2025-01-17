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

(ns util.http-client
  (:require
   [clojure.data.json :as json]
   [clojure.java.io :as io]
   [clojure.string :as string])
  (:import
   (java.io ByteArrayOutputStream InputStream IOException)
   (java.net URI URL HttpURLConnection)))

(set! *warn-on-reflection* true)

;; http client using HttpURLConnection, extracted from clj-http-lite

(defn parse-headers
  "Takes a URLConnection and returns a map of names to values.
   If a name appears more than once (like `set-cookie`) then the value
   will be a vector containing the values in the order they appeared
   in the headers."
  [conn]
  (loop [i 1 headers {}]
    (let [k (.getHeaderFieldKey ^HttpURLConnection conn i)
          v (.getHeaderField ^HttpURLConnection conn i)]
      (if k
        (recur (inc i) (update-in headers [k] conj v))
        (zipmap (for [k (keys headers)]
                  (.toLowerCase ^String k))
                (for [v (vals headers)]
                  (if (= 1 (count v))
                    (first v)
                    (vec v))))))))

(defn- coerce-body-entity
  "Coerce the http-entity from an HttpResponse to either a byte-array, or a
  stream that closes itself and the connection manager when closed."
  [{:keys [as]} conn]
  (let [ins (try
              (.getInputStream ^HttpURLConnection conn)
              (catch Exception e
                (.getErrorStream ^HttpURLConnection conn)))]
    (if (or (= :stream as) (nil? ins))
      ins
      (with-open [ins ^InputStream ins
                  baos (ByteArrayOutputStream.)]
        (io/copy ins baos)
        (.flush baos)
        (.toByteArray baos)))))

(defn split-url
  [url]
  (let [u (URL. url)
        port (.getPort u)]
    (cond-> {:protocol (.getProtocol u)
             :host     (.getHost u)
             :path     (.getPath u)
             :query    (.getQuery u)}
      (not (neg? port)) (assoc :port port))))

(defn request
  "Executes the HTTP request corresponding to the given Ring request map and
   returns the Ring response map corresponding to the resulting HTTP response.
   Note that where Ring uses InputStreams for the request and response bodies,
   the clj-http uses ByteArrays for the bodies."
  [{:keys [request-method scheme server-name server-port uri query-string
           headers content-type character-encoding body socket-timeout
           conn-timeout multipart debug insecure? save-request? follow-redirects
           chunk-size] :as req}]
  (let [http-url (str (name scheme) "://" server-name
                      (when server-port (str ":" server-port))
                      uri
                      (when query-string (str "?" query-string)))
        ^HttpURLConnection conn (.openConnection ^URL (URL. http-url))]
    (when (and content-type character-encoding)
      (.setRequestProperty conn "Content-Type" (str content-type
                                                    "; charset="
                                                    character-encoding)))
    (when (and content-type (not character-encoding))
      (.setRequestProperty conn "Content-Type" content-type))
    (doseq [[h v] headers]
      (.setRequestProperty conn h v))
    (when (false? follow-redirects)
      (.setInstanceFollowRedirects ^HttpURLConnection conn false))
    (.setRequestMethod ^HttpURLConnection conn (.toUpperCase (name request-method)))
    (when body
      (.setDoOutput conn true))
    (when socket-timeout
      (.setReadTimeout conn socket-timeout))
    (when conn-timeout
      (.setConnectTimeout conn conn-timeout))
    (when chunk-size
      (.setChunkedStreamingMode conn chunk-size))
    (.connect conn)
    (when body
      (with-open [out (.getOutputStream conn)]
        (io/copy body out)))
    (merge {:headers (parse-headers conn)
            :status (.getResponseCode ^HttpURLConnection conn)
            :body (when-not (= request-method :head)
                    (coerce-body-entity req conn))}
           (when save-request?
             {:request (assoc (dissoc req :save-request?)
                              :http-url http-url)}))))
