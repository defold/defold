;; Copyright 2020-2023 The Defold Foundation
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

(ns util.num)

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defmacro ubyte [num]
  (if *unchecked-math*
    `(unchecked-byte (unchecked-long ~num))
    `(unchecked-byte
       (let [n# ~num
             ln# (unchecked-long n#)]
         (if (or (< ln# 0) (> ln# 255))
           (throw (IllegalArgumentException. (str "Value out of range for ubyte: " n#)))
           ln#)))))

(defmacro ushort [num]
  (if *unchecked-math*
    `(unchecked-short (unchecked-long ~num))
    `(unchecked-short
       (let [n# ~num
             ln# (unchecked-long n#)]
         (if (or (< ln# 0) (> ln# 65535))
           (throw (IllegalArgumentException. (str "Value out of range for ushort: " n#)))
           ln#)))))

(defmacro uint [num]
  (if *unchecked-math*
    `(unchecked-int (unchecked-long ~num))
    `(unchecked-int
       (let [n# ~num
             ln# (unchecked-long n#)]
         (if (or (< ln# 0) (> ln# 4294967295))
           (throw (IllegalArgumentException. (str "Value out of range for uint: " n#)))
           ln#)))))

(definline normalized->byte [num]
  `(byte (Math/floor (* (double ~num) 127.5))))

(definline normalized->ubyte [num]
  `(ubyte (Math/round (* (double ~num) 255.0))))

(definline normalized->short [num]
  `(short (Math/floor (* (double ~num) 32767.5))))

(definline normalized->ushort [num]
  `(ushort (Math/round (* (double ~num) 65535.0))))

(definline normalized->int [num]
  `(int (Math/floor (* (double ~num) 2147483647.5))))

(definline normalized->uint [num]
  `(uint (Math/round (* (double ~num) 4294967295.0))))
