path = 'demo_level_1.tmap'
with open(path, 'r', encoding='utf-8', newline='') as f:
    s = f.read()

mappings = [
    ('"model": "wall2x2.cmdl"', '"model": "props/building/wall2x2.cmdl"'),
    ('"model": "floor-2x2.cmdl"', '"model": "props/building/floor-2x2.cmdl"'),
    ('"model": "work_prop/gas_cylinder.cmdl"', '"model": "props/gas_cylinder/gas_cylinder.cmdl"'),
    ('"model": "SWAT_Model.cmdl"', '"model": "characters/swat_model/swat_model.cmdl"'),
    ('"model": "car_backup/jeep_wheel.cmdl"', '"model": "car/jeep_wheel.cmdl"'),
    ('"model": "top_down/testcharacter.cmdl"', '"model": "characters/testcharacter/testcharacter.cmdl"'),
    ('"model": "top_down_backup2/chest.cmdl"', '"model": "top_down/chest.cmdl"'),
    ('"model": "work_prop/trolley.cmdl"', '"model": "props/trolley1/trolley.cmdl"'),
    ('"model": "work_prop/work_light.cmdl"', '"model": "props/work_light/work_light.cmdl"'),
]
total = 0
for old, new in mappings:
    c = s.count(old)
    total += c
    s = s.replace(old, new)
    print(f'{c:4d}  {old}')

with open(path, 'w', encoding='utf-8', newline='') as f:
    f.write(s)
print(f'total replacements: {total}')
