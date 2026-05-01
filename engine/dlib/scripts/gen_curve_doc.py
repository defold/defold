# Copyright 2020-2026 The Defold Foundation
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

import json, sys
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from pylab import *

# Generates documentation images using the generated file easing.json, see gen_easing.cpp

def plot(name, values):
    fig = plt.figure()
    fig.suptitle(name, fontsize=12, family='Arial')

    fig.set_figwidth(2)
    fig.set_figheight(2)
    delta = 1.0 / (len(values)-1)
    t = arange(0.0, 1 + delta, delta)

    ax = fig.add_subplot(1,1,1)
    ax.plot(t, values, linewidth=0.6, color='black')

    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    ax.spines['left'].set_visible(False)
    ax.spines['bottom'].set_visible(False)
    ax.get_xaxis().tick_bottom()
    ax.get_yaxis().tick_left()

    axhline(linewidth=0.5, color='black')
    axvline(linewidth=0.5, color='black')

    xticks([])
    yticks([])
    xlim(min(values) - 0.05, max(values))
    ylim(min(values) - 0.05, max(values))

    text(0.85, -0.1, 'Time', color='black', size=8, family='Arial')
    savefig('doc/easing/%s.png' % name.lower())

def gen_doc(filename):
    doc = json.load(open(filename, 'rb'))

    for c in doc:
        print (c['type'])
        values = c['values'][:-1] # Remove last duplicated sample
        plot(c['type'], values)

if __name__ == '__main__':
    matplotlib.rc('xtick', labelsize=8)
    matplotlib.rc('ytick', labelsize=8)
    gen_doc(sys.argv[1])
