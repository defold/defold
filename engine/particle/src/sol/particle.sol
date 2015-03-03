module particle

require io

!export("foo_bar")
function foo_bar() : int
    io.println("FOO BAR JUST WORKS!")
    return 10
end

