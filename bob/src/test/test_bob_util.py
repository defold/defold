from bob import change_ext, task, create_task, add_task
from os.path import splitext

def dynamic_file(prj, tsk):
    with open(tsk['inputs'][0], 'r') as f:
        for i, line in enumerate(f):
            number = int(line)
            output = tsk['outputs'][i]
            with open(output, 'wb') as out_f:
                out_f.write(str(number))

def dynamic_factory(prj, input):
    outputs = []

    tsk = task(function = dynamic_file,
               name = 'dynamic',
               inputs = [input])

    with open(input, 'r') as f:
        for i, line in enumerate(f):
            name, ext = splitext(input)
            number_input = change_ext(prj, '%s_%d' % (name, i), '.number')
            outputs.append(number_input)
            number_tsk = create_task(prj, number_input)
            number_tsk['product_of'] = tsk

    tsk['outputs'] = outputs


    return add_task(prj, tsk)

def number_file(prj, tsk):
    with open(tsk['inputs'][0], 'r') as f:
        buf = f.read()
        with open(tsk['outputs'][0], 'w') as out_f:
            # TODO: 100 as options?
            out_f.write(str(int(buf) * 100))

def number_factory(prj, input):
    return add_task(prj, task(function = number_file,
                              name = 'number',
                              inputs = [input],
                              outputs = [change_ext(prj, input, '.numberc')]))
