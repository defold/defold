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

(ns util.num)

(set! *warn-on-reflection* true)

(def ^:const byte-min-double -128.0)
(def ^:const byte-max-double 127.0)
(def ^:const ubyte-min-double 0.0)
(def ^:const ubyte-max-double 255.0)
(def ^:const short-min-double -32768.0)
(def ^:const short-max-double 32767.0)
(def ^:const ushort-min-double 0.0)
(def ^:const ushort-max-double 65535.0)
(def ^:const int-min-double -2147483648.0)
(def ^:const int-max-double 2147483647.0)
(def ^:const uint-min-double 0.0)
(def ^:const uint-max-double 4294967295.0)
(def ^:const float-min-double -3.4028234663852886E38)
(def ^:const float-max-double 3.4028234663852886E38)

(definline unchecked-ubyte [num]
  `(unchecked-byte (unchecked-short ~num)))

(definline unchecked-ushort [num]
  `(unchecked-short (unchecked-int ~num)))

(definline unchecked-uint [num]
  `(unchecked-int (unchecked-long ~num)))

(definline ubyte [num]
  (if *unchecked-math*
    `(unchecked-ubyte ~num)
    `(unchecked-byte
       (let [n# ~num
             ln# (unchecked-long n#)]
         (if (or (< ln# 0) (> ln# 255))
           (throw (IllegalArgumentException. (str "Value out of range for ubyte: " n#)))
           ln#)))))

(definline ushort [num]
  (if *unchecked-math*
    `(unchecked-ushort ~num)
    `(unchecked-short
       (let [n# ~num
             ln# (unchecked-long n#)]
         (if (or (< ln# 0) (> ln# 65535))
           (throw (IllegalArgumentException. (str "Value out of range for ushort: " n#)))
           ln#)))))

(definline uint [num]
  (if *unchecked-math*
    `(unchecked-uint ~num)
    `(unchecked-int
       (let [n# ~num
             ln# (unchecked-long n#)]
         (if (or (< ln# 0) (> ln# 4294967295))
           (throw (IllegalArgumentException. (str "Value out of range for uint: " n#)))
           ln#)))))

(definline ubyte->long [num]
  `(bit-and (long ~num) 0xff))

(definline ushort->long [num]
  `(bit-and (long ~num) 0xffff))

(definline uint->long [num]
  `(bit-and (long ~num) 0xffffffff))

(definline ubyte->float [num]
  `(float (ubyte->long ~num)))

(definline ushort->float [num]
  `(float (ushort->long ~num)))

(definline uint->float [num]
  `(float (uint->long ~num)))

(definline ubyte->double [num]
  `(double (ubyte->long ~num)))

(definline ushort->double [num]
  `(double (ushort->long ~num)))

(definline uint->double [num]
  `(double (uint->long ~num)))

(definline byte-range->normalized [num]
  `(let [n# (double ~num)]
     (if (pos? n#)
       (/ n# 127.0)
       (/ n# 128.0))))

(definline ubyte-range->normalized [num]
  `(/ (double ~num) ubyte-max-double))

(definline short-range->normalized [num]
  `(let [n# (double ~num)]
     (if (pos? n#)
       (/ n# 32767.0)
       (/ n# 32768.0))))

(definline ushort-range->normalized [num]
  `(/ (double ~num) ushort-max-double))

(definline int-range->normalized [num]
  `(let [n# (double ~num)]
     (if (pos? n#)
       (/ n# 2147483647.0)
       (/ n# 2147483648.0))))

(definline uint-range->normalized [num]
  `(/ (double ~num) uint-max-double))

(definline long-range->normalized [num]
  `(let [n# (double ~num)]
     (if (pos? n#)
       (/ n# 9223372036854775807.0)
       (/ n# 9223372036854775808.0))))

(definline normalized->byte-double [num]
  `(let [n# (double ~num)]
     (Math/rint (if (pos? n#)
                  (* n# 127.0)
                  (* n# 128.0)))))

(definline normalized->ubyte-double [num]
  `(Math/rint (* (double ~num) ubyte-max-double)))

(definline normalized->short-double [num]
  `(let [n# (double ~num)]
     (Math/rint (if (pos? n#)
                  (* n# 32767.0)
                  (* n# 32768.0)))))

(definline normalized->ushort-double [num]
  `(Math/rint (* (double ~num) ushort-max-double)))

(definline normalized->int-double [num]
  `(let [n# (double ~num)]
     (Math/rint (if (pos? n#)
                  (* n# 2147483647.0)
                  (* n# 2147483648.0)))))

(definline normalized->long-double [num]
  `(let [n# (double ~num)]
     (Math/rint (if (pos? n#)
                  (* n# 9223372036854775807.0)
                  (* n# 9223372036854775808.0)))))

(definline normalized->uint-double [num]
  `(Math/rint (* (double ~num) uint-max-double)))

(definline normalized->unchecked-byte [num]
  `(unchecked-byte (normalized->byte-double ~num)))

(definline normalized->unchecked-ubyte [num]
  `(unchecked-ubyte (normalized->ubyte-double ~num)))

(definline normalized->unchecked-short [num]
  `(unchecked-short (normalized->short-double ~num)))

(definline normalized->unchecked-ushort [num]
  `(unchecked-ushort (normalized->ushort-double ~num)))

(definline normalized->unchecked-int [num]
  `(unchecked-int (normalized->int-double ~num)))

(definline normalized->unchecked-uint [num]
  `(unchecked-uint (normalized->uint-double ~num)))

(definline normalized->unchecked-long [num]
  `(unchecked-long (normalized->long-double ~num)))

(definline normalized->byte [num]
  `(byte (normalized->byte-double ~num)))

(definline normalized->ubyte [num]
  `(ubyte (normalized->ubyte-double ~num)))

(definline normalized->short [num]
  `(short (normalized->short-double ~num)))

(definline normalized->ushort [num]
  `(ushort (normalized->ushort-double ~num)))

(definline normalized->int [num]
  `(int (normalized->int-double ~num)))

(definline normalized->uint [num]
  `(uint (normalized->uint-double ~num)))

(definline normalized->long [num]
  (if *unchecked-math*
    `(unchecked-long (normalized->long-double ~num))
    `(long (normalized->long-double ~num))))
