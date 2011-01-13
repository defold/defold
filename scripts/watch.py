import Tkinter
import glob, stat, os

root_path = "content"
dirs = ["prototypes"]

state = {}

def UpdateState():
    changed = False
    for root, dirs, files in os.walk(root_path):
        for f in files:
            name = os.path.join(root, f)
            mtime = os.stat(name)[stat.ST_MTIME]
            oldmtime = state.get(name, 0)
            if mtime != oldmtime:
                changed = True
            state[name] = mtime
    return changed
        
root = Tkinter.Tk()
root.minsize(300, 200)
label = Tkinter.Label()
label.config(text = "Waiting for resources to change")
label.pack()

def tick():
    label.config(text = "Waiting for resources to change")
    if UpdateState():
        label.config(text = "Compiling...")
        label.update()
        result = os.system('waf --skip-tests')
        if result == 0:
            f = open('build/default/content/reload', 'wb')
            f.close()
            label.config(text = "Success!", foreground = "#080")
        else:
            label.config(text = "Failure!", foreground = "#a00")
    label.after(1000, tick)

tick()
label.mainloop()
