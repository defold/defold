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

(ns util.crypto-test
  (:require [clojure.test :refer :all]
            [util.crypto :as crypto]))

(deftest crypto-test

  (testing "Secret key is not allowed to be empty."
    (is (thrown? IllegalArgumentException (crypto/secret-string ""))))

  (testing "Encrypted value is allowed to be empty."
    (is (= ""
           (-> ""
               (crypto/encrypt-string-base64 (crypto/secret-string "Encryption Key"))
               (crypto/decrypt-string-base64 (crypto/secret-string "Encryption Key"))))))

  (testing "Encryption round-trip with same secret key."
    (is (= "Encrypted String"
           (-> "Encrypted String"
               (crypto/encrypt-string-base64 (crypto/secret-string "Encryption Key"))
               (crypto/decrypt-string-base64 (crypto/secret-string "Encryption Key"))))))

  (testing "Secret keys must match."
    (is (not= "Original String"
              (-> "Original String"
                  (crypto/encrypt-string-base64 (crypto/secret-string "Encryption Key"))
                  (crypto/decrypt-string-base64 (crypto/secret-string "Different Key"))))))

  (testing "Secret key can be reused."
    (let [reused-secret-key (crypto/secret-string "Reused Secret Key")]
      (is (= "Encrypted Message"
             (-> "Encrypted Message"
                 (crypto/encrypt-string-base64 reused-secret-key)
                 (crypto/decrypt-string-base64 reused-secret-key))))))

  (testing "Encryption produces a different result every time."
    (let [encrypted-string "Encrypted String"
          secret-key (crypto/secret-string "Password")]
      (is (not= (seq (crypto/encrypt-string-base64 encrypted-string secret-key))
                (seq (crypto/encrypt-string-base64 encrypted-string secret-key)))))))
