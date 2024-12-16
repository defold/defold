# Copyright 2020-2024 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
# 
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
# 
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.

# common utility functions for all bindings generators
import re

re_1d_array = re.compile(r"^(?:const )?\w*\s*\*?\[\d*\]$")
re_2d_array = re.compile(r"^(?:const )?\w*\s*\*?\[\d*\]\[\d*\]$")
re_dmarray = re.compile(r"^\s*dmArray\s*<(.*)>.*$")
re_ptr_type = re.compile(r"^(const )?\s*([\w0-9]+)\s*(\*+)$")

def is_1d_array_type(s):
    return re_1d_array.match(s) is not None

def is_dmarray_type(s):
    return re_dmarray.match(s) is not None

def is_2d_array_type(s):
    return re_2d_array.match(s) is not None

def is_array_type(s):
    return is_1d_array_type(s) or is_2d_array_type(s)

def extract_array_type(s):
    return s[:s.index('[')].strip()

def extract_dmarray_type(s):
    m = re_dmarray.match(s)
    return m.group(1).strip()

def extract_array_sizes(s):
    return s[s.index('['):].replace('[', ' ').replace(']', ' ').split()

def is_string_ptr(s):
    return s == "const char *"

def is_const_void_ptr(s):
    return s == "const void *"

def is_void_ptr(s):
    return s == "void *"

def is_func_ptr(s):
    return '(*)' in s

# https://regex101.com/r/2tP07D/1
def extract_ptr_type(s):
    m = re_ptr_type.match(s)
    if m is not None:
        return '%s %s' % (m.group(2), m.group(3)[1:]) # group 3 may contain "**"
    return s

def extract_ptr_type2(s):
    m = re_ptr_type.match(s)
    if m is not None:
        return (m.group(2), m.group(3)) # ("Vec2f", "**")
    return (s, '')

# PREFIX_BLA_BLUB to bla_blub
def as_lower_snake_case(s, prefix):
    outp = s.lower()
    if outp.startswith(prefix):
        outp = outp[len(prefix):]
    return outp

# prefix_bla_blub => blaBlub, PREFIX_BLA_BLUB => blaBlub
def as_lower_camel_case(s, prefix):
    outp = s.lower()
    if outp.startswith(prefix):
        outp = outp[len(prefix):]
    parts = outp.split('_')
    outp = parts[0]
    for part in parts[1:]:
        outp += part.capitalize()
    return outp
