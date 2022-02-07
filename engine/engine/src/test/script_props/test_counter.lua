local M = {}

local started_test_scripts = 0

function M.start()
    started_test_scripts = started_test_scripts + 1
end

function M.done()
    timer.delay(0, false, function()
        started_test_scripts = started_test_scripts - 1
        if started_test_scripts == 0 then
            msg.post("main:/main#script", "done")
        end
    end)
end

return M
