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

(ns util.crypto
  (:import [java.security GeneralSecurityException SecureRandom]
           [java.util Arrays Base64]
           [javax.crypto Cipher KeyGenerator]
           [javax.crypto.spec IvParameterSpec SecretKeySpec]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- bytes->string
  ^String [^bytes bytes]
  (String. bytes "UTF-8"))

(defn- string->bytes
  ^bytes [^String string]
  (.getBytes string "UTF-8"))

(defn- bytes->base64
  ^String [^bytes bytes]
  (.encodeToString (Base64/getUrlEncoder) bytes))

(defn base64->bytes
  ^bytes [^String base64-string]
  (.decode (Base64/getUrlDecoder) base64-string))

(defn- random-bytes
  ^bytes [^long size]
  (let [random (SecureRandom/getInstance "SHA1PRNG")
        random-bytes (byte-array size)]
    (.nextBytes random random-bytes)
    random-bytes))

(defn- initialization-vector
  ^IvParameterSpec [^bytes initialization-vector-bytes]
  (IvParameterSpec. initialization-vector-bytes))

(defn- cipher
  ^Cipher [mode ^SecretKeySpec secret-key ^IvParameterSpec initialization-vector]
  (doto (Cipher/getInstance "AES/CBC/PKCS5Padding")
    (.init (int mode) secret-key initialization-vector)))

(defn secret-bytes
  ^SecretKeySpec [^bytes secret & additional-secrets]
  (if (empty? secret)
    (throw (IllegalArgumentException. "Secret cannot be empty."))
    (let [random (doto (SecureRandom/getInstance "SHA1PRNG")
                   (.setSeed secret))]
      ;; When the setSeed method is called multiple times, each additional
      ;; seed supplements, rather than replaces, the existing seed.
      (doseq [^bytes additional-secret additional-secrets]
        (.setSeed random additional-secret))
      (-> (doto (KeyGenerator/getInstance "AES")
            (.init 128 random))
          (.generateKey)
          (.getEncoded)
          (SecretKeySpec. "AES")))))

(defn secret-string
  ^SecretKeySpec [^String secret & additional-secrets]
  (apply secret-bytes
         (string->bytes secret)
         (map string->bytes additional-secrets)))

(defn encrypt-bytes
  ^bytes [^bytes value-bytes ^SecretKeySpec secret-key]
  (let [initialization-vector-bytes (random-bytes 16)
        initialization-vector (initialization-vector initialization-vector-bytes)
        cipher (cipher Cipher/ENCRYPT_MODE secret-key initialization-vector)]
    (byte-array
      (concat initialization-vector-bytes
              (.doFinal cipher value-bytes)))))

(defn decrypt-bytes
  ^bytes [^bytes encrypted-bytes ^SecretKeySpec secret-key]
  (let [initialization-vector-bytes (Arrays/copyOfRange encrypted-bytes 0 16)
        encrypted-value-bytes (Arrays/copyOfRange encrypted-bytes 16 (count encrypted-bytes))
        initialization-vector (initialization-vector initialization-vector-bytes)
        cipher (cipher Cipher/DECRYPT_MODE secret-key initialization-vector)]
    ;; According to the contract we should return garbled output if the keys do
    ;; not match. However, decryption can fail internally due to padding issues.
    ;; In that case we simply return the encrypted bytes unaltered.
    (try
      (.doFinal cipher encrypted-value-bytes)
      (catch GeneralSecurityException _
        encrypted-bytes))))

(defn encrypt-bytes-base64
  ^String [^bytes value-bytes ^SecretKeySpec secret-key]
  (-> value-bytes
      (encrypt-bytes secret-key)
      bytes->base64))

(defn decrypt-bytes-base64
  ^bytes [^String encrypted-string ^SecretKeySpec secret-key]
  (-> encrypted-string
      base64->bytes
      (decrypt-bytes secret-key)))

(defn encrypt-string-base64
  ^String [^String value-string ^SecretKeySpec secret-key]
  (-> value-string
      string->bytes
      (encrypt-bytes-base64 secret-key)))

(defn decrypt-string-base64
  ^String [^String encrypted-string ^SecretKeySpec secret-key]
  (-> encrypted-string
      (decrypt-bytes-base64 secret-key)
      bytes->string))
