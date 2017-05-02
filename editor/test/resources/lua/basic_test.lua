table = {
    string = "value",
    boolean = true,
    int = 1,
    double = 1.1
}

array = {1, 2, "value"}

mixed_table = {1, 2, string = "value"}

function add_func(a, b)
    return a + b
end

function add_func_array_arg(args)
    return args[1] + args[2]
end

function add_func_table_arg(args)
    return args.a + args.b
end
