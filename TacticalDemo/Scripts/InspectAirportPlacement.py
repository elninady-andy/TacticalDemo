"""Read-only airport geometry inspection; writes reports only under Saved/Evidence."""
import json
import os
import unreal


def vec(value):
    return {"x": float(value.x), "y": float(value.y), "z": float(value.z)}


def rot(value):
    return {"pitch": float(value.pitch), "yaw": float(value.yaw), "roll": float(value.roll)}


MAP = "/Game/VRAirport/Maps/VRAirportMap"
if not unreal.EditorLevelLibrary.load_level(MAP):
    raise RuntimeError("Cannot load " + MAP)
world = unreal.EditorLevelLibrary.get_editor_world()
report = {"map": MAP, "units": "centimeters", "actors": [], "models": [], "traces": [], "geometry": []}
actors = unreal.EditorLevelLibrary.get_all_level_actors()
for actor in actors:
    origin, extent = actor.get_actor_bounds(False)
    entry = {
        "name": actor.get_name(), "label": actor.get_actor_label(),
        "class": actor.get_class().get_name(), "location": vec(actor.get_actor_location()),
        "rotation": rot(actor.get_actor_rotation()), "scale": vec(actor.get_actor_scale3d()),
        "boundsOrigin": vec(origin), "boundsExtent": vec(extent), "components": [],
    }
    for component in actor.get_components_by_class(unreal.StaticMeshComponent):
        mesh = component.get_editor_property("static_mesh")
        if not mesh:
            continue
        local_bounds = mesh.get_bounds()
        entry["components"].append({
            "name": component.get_name(), "mesh": mesh.get_path_name(),
            "materials": [str(component.get_material(i).get_path_name()) if component.get_material(i) else None
                          for i in range(component.get_num_materials())],
            "collisionEnabled": str(component.get_collision_enabled()),
            "location": vec(component.get_world_location()), "rotation": rot(component.get_world_rotation()),
            "scale": vec(component.get_world_scale()),
            "meshBoundsOrigin": vec(local_bounds.origin), "meshBoundsExtent": vec(local_bounds.box_extent),
        })
    report["actors"].append(entry)

for path in ["/Game/plane/J20/mesh/J20S", "/Game/plane/J10/mesh/J10B"]:
    mesh = unreal.load_asset(path)
    bounds = mesh.get_bounds()
    model = {"asset": path, "origin": vec(bounds.origin), "extent": vec(bounds.box_extent),
             "lowestZ": float(bounds.origin.z - bounds.box_extent.z)}
    lowest = model["lowestZ"]
    contacts = []
    for index in range(mesh.get_num_sections(0)):
        vertices, triangles, normals, uvs, tangents = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh, 0, index)
        contacts.extend(vec(vertex) for vertex in vertices if vertex.z <= lowest + 3)
    model["lowestVerticesWithin3cm"] = contacts
    report["models"].append(model)

for actor in actors:
    if actor.get_actor_label() not in ["paodao", "Airport_DrawCall_0008", "Airport_Object854", "plane002_Plane018", "plane002_Plane019"]:
        continue
    component = actor.get_components_by_class(unreal.StaticMeshComponent)[0]
    mesh = component.get_editor_property("static_mesh")
    geometry = {"actor": actor.get_actor_label(), "sections": []}
    try:
        desc = mesh.get_static_mesh_description(0)
        geometry["sectionCount"] = mesh.get_num_sections(0)
        for index in range(mesh.get_num_sections(0)):
            vertices, triangles, normals, uvs, tangents = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh, 0, index)
            transform = component.get_world_transform()
            transformed = [vec(unreal.MathLibrary.transform_location(transform, vertex)) for vertex in vertices]
            section = {"index": index, "vertices": transformed, "triangles": list(triangles)}
            try:
                material_index = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).get_lod_material_slot(mesh, 0, index)
                material = component.get_material(material_index)
                section["materialIndex"] = material_index
                section["material"] = material.get_path_name() if material else None
            except Exception as exc:
                section["materialSlotInspectionError"] = str(exc)
            geometry["sections"].append(section)
    except Exception as exc:
        geometry["error"] = str(exc)
    report["geometry"].append(geometry)

report["groundMaterials"] = []
for path in ["/Game/VRAirport/Materials/Material__26667", "/Game/VRAirport/Materials/Material__2"]:
    material = unreal.load_asset(path)
    nodes = []
    pending = [unreal.MaterialEditingLibrary.get_material_property_input_node(material, unreal.MaterialProperty.MP_BASE_COLOR)]
    seen = set()
    while pending:
        node = pending.pop()
        if not node or node.get_path_name() in seen:
            continue
        seen.add(node.get_path_name())
        row = {"path": node.get_path_name(), "class": node.get_class().get_name()}
        if isinstance(node, unreal.MaterialExpressionTextureSample):
            row["texture"] = str(node.get_editor_property("texture"))
        nodes.append(row)
        pending.extend(unreal.MaterialEditingLibrary.get_inputs_for_material_expression(material, node))
    report["groundMaterials"].append({"material": path, "baseColorNodes": nodes})


def triangle_ground(x, y, section):
    vertices = section["vertices"]
    indices = section["triangles"]
    highest_z = None
    for index in range(0, len(indices), 3):
        a, b, c = [vertices[indices[index + offset]] for offset in range(3)]
        denominator = (b["y"] - c["y"]) * (a["x"] - c["x"]) + (c["x"] - b["x"]) * (a["y"] - c["y"])
        if abs(denominator) < 0.0001:
            continue
        alpha = ((b["y"] - c["y"]) * (x - c["x"]) + (c["x"] - b["x"]) * (y - c["y"])) / denominator
        beta = ((c["y"] - a["y"]) * (x - c["x"]) + (a["x"] - c["x"]) * (y - c["y"])) / denominator
        gamma = 1 - alpha - beta
        if min(alpha, beta, gamma) >= -0.000001:
            z = alpha * a["z"] + beta * b["z"] + gamma * c["z"]
            highest_z = z if highest_z is None else max(z, highest_z)
    return highest_z


concrete = next(geometry for geometry in report["geometry"] if geometry["actor"] == "paodao")["sections"][0]
report["proposedPlacement"] = []
for model_index, x in [(0, 55000), (1, 58500)]:
    model = report["models"][model_index]
    for y in [-44500, -48000, -51500]:
        samples = []
        for dx, dy in [(0, 0), (-1, -1), (-1, 1), (1, -1), (1, 1)]:
            sample_x = x + model["origin"]["x"] + dx * model["extent"]["x"]
            sample_y = y + model["origin"]["y"] + dy * model["extent"]["y"]
            samples.append({"x": sample_x, "y": sample_y, "groundZ": triangle_ground(sample_x, sample_y, concrete)})
        ground_z = triangle_ground(x, y, concrete)
        actor_z = ground_z - model["lowestZ"] if ground_z is not None else None
        overlaps = []
        for actor in report["actors"]:
            if not actor["components"] or actor["boundsExtent"]["z"] < 80 or actor["label"] in ["SkySpherePC2", "Airport_Plane006"]:
                continue
            actor_min_x = actor["boundsOrigin"]["x"] - actor["boundsExtent"]["x"]
            actor_max_x = actor["boundsOrigin"]["x"] + actor["boundsExtent"]["x"]
            actor_min_y = actor["boundsOrigin"]["y"] - actor["boundsExtent"]["y"]
            actor_max_y = actor["boundsOrigin"]["y"] + actor["boundsExtent"]["y"]
            model_min_x = x + model["origin"]["x"] - model["extent"]["x"]
            model_max_x = x + model["origin"]["x"] + model["extent"]["x"]
            model_min_y = y + model["origin"]["y"] - model["extent"]["y"]
            model_max_y = y + model["origin"]["y"] + model["extent"]["y"]
            if model_min_x < actor_max_x and model_max_x > actor_min_x and model_min_y < actor_max_y and model_max_y > actor_min_y:
                overlaps.append(actor["label"])
        report["proposedPlacement"].append({"model": model["asset"], "position": {"x": x, "y": y, "z": actor_z},
            "concreteFootprintSamples": samples, "groundZ": ground_z, "existingObstacleBoundsOverlaps": overlaps})

report["traceDocumentation"] = unreal.SystemLibrary.line_trace_single.__doc__
for x, y in [(56000, -41000), (61000, -41000), (56000, -38000), (61000, -38000)]:
    try:
        result = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 20000),
                    unreal.Vector(x, y, -20000), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
                    True, [], unreal.DrawDebugTrace.NONE, True)
        trace = {"x": x, "y": y, "result": str(result)}
        report["traces"].append(trace)
    except Exception as exc:
        report["traces"].append({"x": x, "y": y, "error": str(exc)})

report["availableGeometryAPIs"] = [name for name in dir(unreal) if "StaticMeshExporter" in name or "ProceduralMesh" in name]
report["meshGeometryMethods"] = [name for name in dir(mesh) if "description" in name.lower() or "vertex" in name.lower() or "lod" in name.lower()]
target = os.path.join(unreal.Paths.project_saved_dir(), "Evidence", "AirportPlacement.json")
os.makedirs(os.path.dirname(target), exist_ok=True)
with open(target, "w", encoding="utf-8") as stream:
    json.dump(report, stream, ensure_ascii=False, indent=2)
unreal.log("AIRPORT_PLACEMENT_INSPECTED actors=" + str(len(actors)) + " report=" + target)
