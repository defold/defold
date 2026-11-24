-- run from coroutine
local function test()
	local co = coroutine.running()
	call_callback(function ()
		coroutine.resume(co)
		assert(coroutine.status(co) == "dead")
	end)
	coroutine.yield(co)
end

global_co = coroutine.create(test)
local success, msg = coroutine.resume(global_co)
if not success then
	error(msg)
end
