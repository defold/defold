function test_job()

    local f = function()
        print("LUA CALLBACK!")
    end

    local b = buffer.create( 1024, { {name=hash("v"), type=buffer.VALUE_TYPE_FLOAT32, count=1 } })
    local j = job.new(f, {10,20,30,b})
--    local j = job.new(job.hack, {10,20,30,b})
    print(j)
    job.run(j)

    local root = job.new(f, {10,20,30,b})
    for i = 1,10 do
        local j = job.new(job.hack, {10,20,30,b}, root)
        job.run(j)
    end
    job.run(root)
end

functions = { test_job = test_job }
