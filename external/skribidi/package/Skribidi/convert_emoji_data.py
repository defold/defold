import os

emoji = []
emoji_presentation = []
emoji_modifier_base = []
emoji_component = []
extended_pictographic = []


with open('emoji-data.txt', 'r', encoding='utf-8') as file:
    # Read each line in the file
    for line in file:
        # Skip empty and comment
        if len(line) < 1:
            continue
        if line[0] == '#':
            continue
           
        range_end_idx = line.find(';')
        desc_end_idx = line.find('#')
        if range_end_idx == -1 or desc_end_idx == -1:
            continue
            
        range_str = line[0:range_end_idx].strip()
        desc_str = line[range_end_idx+1:desc_end_idx].strip()

        range_min = 0;
        range_max = 0;
        range_sep_idx = range_str.find('..')
        if range_sep_idx != -1:
            range_min = int(range_str[0:range_sep_idx], 16)
            range_max = int(range_str[range_sep_idx+2:], 16)
        else:
            range_min = int(range_str, 16)
            range_max = range_min

        if desc_str == 'Emoji':
            emoji.append((range_min, range_max))
        elif desc_str == 'Emoji_Presentation':
            emoji_presentation.append((range_min, range_max))
        elif desc_str == 'Emoji_Modifier_Base':
            emoji_modifier_base.append((range_min, range_max))
        elif desc_str == 'Emoji_Component':
            emoji_component.append((range_min, range_max))
        elif desc_str == 'Extended_Pictographic':
            extended_pictographic.append((range_min, range_max))
        elif desc_str == 'Emoji_Modifier':
            pass # handle manually
        else:
            print('Unhandled %s' %desc_str)

#        print("[%x-%x]%s, %s" %(range_min, range_max, range_str, desc_str))
            
        # 1F612..1F614  ; Extended_Pictographic#
            
#        print(line.strip())


emoji.sort(key=lambda x:x[0])
emoji_presentation.sort(key=lambda x:x[0])
emoji_modifier_base.sort(key=lambda x:x[0])
emoji_component.sort(key=lambda x:x[0])
extended_pictographic.sort(key=lambda x:x[0])

def dump_data(name, ranges):
    f.write('static const uint32_t %s_ranges[] = {\n' %(name))

    # merge adjacent ranges
    merged_ranges = []    
    for r in ranges:
        if len(merged_ranges) and merged_ranges[-1][1] == (r[0] - 1):
            merged_ranges[-1] = (merged_ranges[-1][0], r[1])
        else:
            merged_ranges.append(r)
    
    # find data range    
    range_min = merged_ranges[0][0]
    range_max = merged_ranges[0][0]
    for r in merged_ranges:
        range_min = min(range_min, min(r[0],r[1]))
        range_max = max(range_max, max(r[0],r[1]))
    
    # write out
    i = 0
    f.write('    ')
    for r in merged_ranges:
        f.write('0x%x,0x%x, ' %(r[0], r[1]))
        i = i+1
        if (i >= 10):
            f.write('\n    ')
            i = 0;
    f.write('};\n')
    f.write('static const int32_t %s_count = %d;\n' %(name, len(merged_ranges)));
    f.write('static const uint32_t %s_min = 0x%x;\n' %(name, range_min));
    f.write('static const uint32_t %s_max = 0x%x;\n' %(name, range_max));
    f.write('\n')
    

with open("src/emoji_data.h", "w") as f:

    f.write('//\n')
    f.write('// Genereted with convert_emoji_data.py, do not edit.\n')
    f.write('//\n')

    dump_data('emoji', emoji)
    dump_data('emoji_presentation', emoji_presentation)
    dump_data('emoji_modifier_base', emoji_modifier_base)
    dump_data('emoji_component', emoji_component)
    dump_data('extended_pictographic', extended_pictographic)
