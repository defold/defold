;; Copyright 2020-2026 The Defold Foundation
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

(ns editor.url
  (:import (java.io IOException)
           (java.net HttpURLConnection MalformedURLException URISyntaxException URI URL)
           (java.nio.charset Charset)))

(defn reachable?
  [^URI uri]
  (let [url (.toURL uri)
        conn (doto ^HttpURLConnection (.openConnection url)
               (.setAllowUserInteraction false)
               (.setInstanceFollowRedirects false)
               (.setConnectTimeout 2000)
               (.setReadTimeout 2000)
               (.setUseCaches false))]
    (try
      (.connect conn)
      (.getContentLength conn)
      true
      (catch IOException _
        false)
      (finally
        (.disconnect conn)))))

(defn strip-path
  ^URI [^URI uri]
  (URI. (.getScheme uri) nil (.getHost uri) (.getPort uri) nil nil nil))

(defn try-parse
  ^URI [^String s]
  (try (.toURI (URL. s))
       (catch MalformedURLException _ nil)
       (catch URISyntaxException _ nil)))


(defn x-www-form-urlencoded-safe-character?
  "Safe characters for x-www-form-urlencoded data, as per java.net.URLEncoder
  and browser behaviour, i.e. alphanumeric plus '*', '-', '.', '_' characters."
  [^long char]
  (case char
    (42 45 46 95) true      ; "*-._"
    (or (<= 48 char 57)     ; 0 - 9
        (<= 65 char 90)     ; A - Z
        (<= 97 char 122)))) ; a - z

(defn encode
  "Percent-encode content into a format suitable for use in an HTTP PUT or HTTP POST.
  See: https://en.wikipedia.org/wiki/Percent-encoding

  This implementation provides additional flexibility compared to using the
  (java.net.URLEncoder/encode ...) function, most notably the ability to encode
  space characters as '%20' instead of '+'.

  Ported to Clojure from this implementation:
  https://svn.apache.org/repos/asf/httpcomponents/httpclient/tags/4.5.3//httpclient/src/main/java/org/apache/http/client/utils/URLEncodedUtils.java"
  ^String [safe-character? blank-as-plus? ^Charset charset ^String content]
  (assert (ifn? safe-character?))
  (assert (not (ifn? blank-as-plus?)))
  (when (some? content)
    (let [builder (StringBuilder.)
          byte-buffer (.encode charset content)]
      (while (.hasRemaining byte-buffer)
        (let [character (bit-and (.get byte-buffer) 0xff)]
          (cond
            (safe-character? character)
            (.append builder (char character))

            (and blank-as-plus? (= 32 character))
            (.append builder \+)

            :else
            (let [hex1 (Character/toUpperCase (Character/forDigit (bit-and (bit-shift-right character 4) 0xf) 16))
                  hex2 (Character/toUpperCase (Character/forDigit (bit-and character 0xf) 16))]
              (.append builder \%)
              (.append builder hex1)
              (.append builder hex2)))))
      (.toString builder))))
