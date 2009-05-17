def Init(self):
    self["Counter"] = 0

def Update(self):
    print "Counter", self["Counter"]
    self["Counter"] += 1