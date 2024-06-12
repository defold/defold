-- The MIT License (MIT)
--
-- Copyright (c) 2014 Alban Linard
--
-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the "Software"), to deal
-- in the Software without restriction, including without limitation the rights
-- to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
-- copies of the Software, and to permit persons to whom the Software is
-- furnished to do so, subject to the following conditions:
--
-- The above copyright notice and this permission notice shall be included in all
-- copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
-- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
-- OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
-- SOFTWARE.

local select      = select
local create      = coroutine.create
local resume      = coroutine.resume
local running     = coroutine.running
local status      = coroutine.status
local wrap        = coroutine.wrap
local yield       = coroutine.yield

return function (tag)
  local coroutine = {
    running     = running,
    status      = status,
  }
  tag = tag or {}

  local function for_wrap (co, ...)
    if tag == ... then
      return select (2, ...)
    else
      return for_wrap (co, co (yield (...)))
    end
  end

  local function for_resume (co, st, ...)
    if not st then
      return st, ...
    elseif tag == ... then
      return st, select (2, ...)
    else
      return for_resume (co, resume (co, yield (...)))
    end
  end

  function coroutine.create (f)
    return create (function (...)
      return tag, f (...)
    end)
  end

  function coroutine.resume (co, ...)
    return for_resume (co, resume (co, ...))
  end

  function coroutine.wrap (f)
    local co = wrap (function (...)
      return tag, f (...)
    end)
    return function (...)
      return for_wrap (co, co (...))
    end
  end

  function coroutine.yield (...)
    return yield (tag, ...)
  end

  return coroutine
end