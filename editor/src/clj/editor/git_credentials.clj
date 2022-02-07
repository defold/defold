;; Copyright 2020 The Defold Foundation
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

(ns editor.git-credentials
  (:require [editor.prefs :as prefs]
            [util.crypto :as crypto])
  (:import [org.apache.commons.codec.digest DigestUtils]
           [java.util Arrays]
           [org.eclipse.jgit.api Git]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^:private project-git-credentials-prefs-key "project-git-credentials")

(def ^:private get-git-credentials-secret-key
  (memoize
    (fn get-git-credentials-secret-key []
      (crypto/secret-bytes (.getBytes (System/getProperty "user.name") "UTF-8")
                           (byte-array [0x9F 0x4B 0x7E 0x22 0xED 0xC1 0xA5 0x71
                                        0x4E 0x96 0xC0 0xA7 0xD4 0xFD 0xBF 0x11
                                        0x41 0xD0 0xE8 0x2D 0x25 0xAB 0x7F 0xBA
                                        0x69 0x33 0xBB 0x47 0xBF 0xC2 0x4D 0x39])))))

(defn credentials? [value]
  (and (map? value)
       (string? (:username value))
       (if-some [[_ password] (find value :password)]
         (string? password)
         true)
       (if-some [[_ ssh-session-password] (find value :ssh-session-password)]
         (string? ssh-session-password)
         true)))

(defn encrypted-credentials? [value]
  (and (map? value)
       (string? (not-empty (:encrypted-checksum value)))
       (string? (not-empty (:encrypted-username value)))
       (if-some [[_ encrypted-password] (find value :encrypted-password)]
         (string? (not-empty encrypted-password))
         true)
       (if-some [[_ encrypted-ssh-session-password] (find value :encrypted-ssh-session-password)]
         (string? (not-empty encrypted-ssh-session-password))
         true)))

(defn encrypt-credentials [credentials]
  {:pre [(or (nil? credentials) (credentials? credentials))]
   :post [(or (nil? %) (encrypted-credentials? %))]}
  (when-some [{:keys [username password ssh-session-password]} credentials]
    (let [secret-key (get-git-credentials-secret-key)
          checksum (DigestUtils/sha1 (str username password ssh-session-password))
          encrypted-checksum (crypto/encrypt-bytes-base64 checksum secret-key)
          encrypted-username (crypto/encrypt-string-base64 username secret-key)
          encrypted-password (some-> password (crypto/encrypt-string-base64 secret-key))
          encrypted-ssh-session-password (some-> ssh-session-password (crypto/encrypt-string-base64 secret-key))]
      (cond-> {:encrypted-checksum encrypted-checksum
               :encrypted-username encrypted-username}
              (some? encrypted-password) (assoc :encrypted-password encrypted-password)
              (some? encrypted-ssh-session-password) (assoc :encrypted-ssh-session-password encrypted-ssh-session-password)))))

(defn decrypt-credentials [encrypted-credentials]
  {:pre [(or (nil? encrypted-credentials) (encrypted-credentials? encrypted-credentials))]
   :post [(or (nil? %) (credentials? %))]}
  (when-some [{:keys [encrypted-checksum encrypted-username encrypted-password encrypted-ssh-session-password]} encrypted-credentials]
    (let [secret-key (get-git-credentials-secret-key)
          username (crypto/decrypt-string-base64 encrypted-username secret-key)
          password (some-> encrypted-password (crypto/decrypt-string-base64 secret-key))
          ssh-session-password (some-> encrypted-ssh-session-password (crypto/decrypt-string-base64 secret-key))
          checksum (crypto/decrypt-bytes-base64 encrypted-checksum secret-key)]
      (when (Arrays/equals checksum (DigestUtils/sha1 (str username password ssh-session-password)))
        (cond-> {:username username}
                (some? password) (assoc :password password)
                (some? ssh-session-password) (assoc :ssh-session-password ssh-session-password))))))

(defn read-encrypted-credentials [prefs ^Git git]
  {:post [(or (nil? %) (encrypted-credentials? %))]}
  (let [encrypted-credentials-by-project-path (prefs/get-prefs prefs project-git-credentials-prefs-key nil)
        project-path (.. git getRepository getWorkTree getCanonicalPath)]
    (get encrypted-credentials-by-project-path project-path)))

(defn write-encrypted-credentials! [prefs ^Git git encrypted-credentials]
  {:pre [(or (nil? encrypted-credentials) (encrypted-credentials? encrypted-credentials))]}
  (let [encrypted-credentials-by-project-path (prefs/get-prefs prefs project-git-credentials-prefs-key {})
        project-path (.. git getRepository getWorkTree getCanonicalPath)]
    (prefs/set-prefs prefs project-git-credentials-prefs-key
                     (if (nil? encrypted-credentials)
                       (dissoc encrypted-credentials-by-project-path project-path)
                       (assoc encrypted-credentials-by-project-path
                         project-path encrypted-credentials)))))
