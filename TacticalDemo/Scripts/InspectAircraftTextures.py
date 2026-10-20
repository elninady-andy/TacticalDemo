import unreal,json,os
report=[]
for path in ['/Game/plane/J20/materials/03_-_Default','/Game/plane/J10/materials/02_-_Default','/Game/plane/J10/materials/03_-_Default']:
    material=unreal.load_asset(path)
    nodes=[]; pending=[unreal.MaterialEditingLibrary.get_material_property_input_node(material,unreal.MaterialProperty.MP_BASE_COLOR)]; seen=set()
    while pending:
        node=pending.pop()
        if not node or node.get_path_name() in seen: continue
        seen.add(node.get_path_name()); row={'path':node.get_path_name(),'class':node.get_class().get_name()}
        if isinstance(node,unreal.MaterialExpressionTextureSample): row['texture']=str(node.get_editor_property('texture'))
        nodes.append(row); pending.extend(unreal.MaterialEditingLibrary.get_inputs_for_material_expression(material,node))
    report.append({'material':path,'nodes':nodes})
for path in ['/Game/plane/J20/texture/mat_J20_Airframe_Diffuse','/Game/plane/J20/texture/mat_J20_Airframe_Diffuse2','/Game/plane/J10/texture/j10b_d','/Game/plane/J10/texture/j10b_d2']:
    t=unreal.load_asset(path)
    report.append({'texture':path,'class':t.get_class().get_name() if t else None})
target=os.path.join(unreal.Paths.project_saved_dir(),'Evidence','AircraftTextureInspection.json')
with open(target,'w',encoding='utf-8') as f:json.dump(report,f,ensure_ascii=False,indent=2)
unreal.log('AIRCRAFT_TEXTURE_INSPECTION '+json.dumps(report,ensure_ascii=False))
