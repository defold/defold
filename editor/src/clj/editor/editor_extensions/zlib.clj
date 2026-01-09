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

(ns editor.editor-extensions.zlib
  (:require [clojure.java.io :as io])
  (:import [com.defold.editor.luart DefoldOneArgLuaFn]
           [java.io ByteArrayInputStream ByteArrayOutputStream]
           [java.util.zip Deflater DeflaterOutputStream GZIPInputStream Inflater InflaterInputStream]
           [org.luaj.vm2 LuaError LuaString LuaValue]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- gzip? [^LuaString lua-string]
  (and (<= 2 (.-m_length lua-string))
       (= 0x1f (bit-and 0xff (aget (.-m_bytes lua-string) (.-m_offset lua-string))))
       (= 0x8b (bit-and 0xff (aget (.-m_bytes lua-string) (unchecked-inc-int (.-m_offset lua-string)))))))

(defn- inflate-gzip [^LuaString lua-string]
  (with-open [in (GZIPInputStream. (ByteArrayInputStream. (.-m_bytes lua-string) (.-m_offset lua-string) (.-m_length lua-string)))
              out (ByteArrayOutputStream.)]
    (io/copy in out)
    (.toByteArray out)))

(defn- inflate-zlib [^LuaString lua-string]
  (with-open [in (InflaterInputStream.
                   (ByteArrayInputStream. (.-m_bytes lua-string) (.-m_offset lua-string) (.-m_length lua-string))
                   (Inflater.))
              out (ByteArrayOutputStream.)]
    (io/copy in out)
    (.toByteArray out)))

(defn- ext-inflate [^LuaValue lua-buf]
  (let [lua-string (.checkstring lua-buf)
        bytes (try
                (if (gzip? lua-string)
                  (inflate-gzip lua-string)
                  (inflate-zlib lua-string))
                (catch Throwable e
                  (throw (LuaError. (format "Failed to inflate buffer (%s)" (.getMessage e))))))]
    (LuaString/valueOf ^byte/1 bytes)))

(defn- deflate-bytes
  [^LuaString lua-string]
  (with-open [out (ByteArrayOutputStream.)
              stream (DeflaterOutputStream. out (Deflater. #_level 3))]
    (.write stream (.-m_bytes lua-string) (.-m_offset lua-string) (.-m_length lua-string))
    (.finish stream)
    (.toByteArray out)))

(defn- ext-deflate [^LuaValue lua-buf]
  (let [lua-string (.checkstring lua-buf)]
    (try
      (LuaString/valueOf ^byte/1 (deflate-bytes lua-string))
      (catch Throwable e
        (throw (LuaError. (format "Failed to deflate buffer (%s)" (.getMessage e))))))))

(def env
  {"inflate" (DefoldOneArgLuaFn. ext-inflate)
   "deflate" (DefoldOneArgLuaFn. ext-deflate)})
