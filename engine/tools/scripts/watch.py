#!/usr/bin/env python

import Tkinter
import glob, stat, os, urllib

root_path = "content"

state = {}

def UpdateState():
    changed = []
    for path, dirs, files in os.walk(root_path):
        (root, sep, sub_path) = path.partition(os.path.sep)
        for f in files:
            name = os.path.join(path, f)
            mtime = os.stat(name)[stat.ST_MTIME]
            oldmtime = state.get(name, 0)
            if mtime != oldmtime:
                changed.append(os.path.join(sub_path, f + "c"))
            state[name] = mtime
    return changed

root = Tkinter.Tk()
root.minsize(300, 200)
label = Tkinter.Label()
label.config(text = "Waiting for resources to change")
label.pack()
UpdateState()

def tick():
    label.config(text = "Waiting for resources to change")
    changed = UpdateState()
    if len(changed) > 0:
        label.config(text = "Compiling...")
        label.update()
        result = os.system('waf --skip-tests')
        if result == 0:
            try:
                for f in changed:
                    print("Reloading: " + f)
                    request = urllib.urlopen("http://localhost:8001/reload/" + f)
                label.config(text = "Success!", foreground = "#080")
            except:
                label.config(text = "No connection!", foreground = "#880")
        else:
            label.config(text = "Failure!", foreground = "#a00")
    label.after(1000, tick)

tick()
label.mainloop()
