local M = {}

M.test_count = 3

function M.done()
	M.test_count = M.test_count - 1
	if M.test_count == 0 then
        msg.post("main:/main#script", "done")
	end
end

return M
