def Init(self):
    self.Counter = 0
    assert self.Position[0] == 0
    assert self.Position[1] == 0
    assert self.Position[2] == 0
    self.Position = (1,2,3)

def Update(self):
    assert self.Position[0] == 1
    assert self.Position[1] == 2
    assert self.Position[2] == 3
    self.Counter += 1
