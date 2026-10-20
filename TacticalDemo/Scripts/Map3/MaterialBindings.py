"""Reuse authoritative airport instance materials without editing original assets."""
import unreal


def capture_airport_bindings():
    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not level.load_level("/Game/VRAirport/Maps/VRAirportMap"):
        raise RuntimeError("Cannot read original airport material overrides")
    result = {}
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        if not isinstance(actor, unreal.StaticMeshActor):
            continue
        component = actor.static_mesh_component
        if not component.static_mesh:
            continue
        result[actor.get_actor_label()] = {
            "mesh": component.static_mesh.get_path_name(),
            "materials": [component.get_material(i).get_path_name() if component.get_material(i) else None
                          for i in range(component.get_num_materials())],
        }
    return result


def apply_airport_binding(actor, bindings):
    name = actor.get_actor_label()
    binding = bindings[name]
    component = actor.static_mesh_component
    if binding["mesh"] != component.static_mesh.get_path_name() or len(binding["materials"]) != component.get_num_materials():
        raise RuntimeError("Original material slots do not match reused mesh: " + name)
    fallbacks = []
    for index, path in enumerate(binding["materials"]):
        if path is None:
            path = "/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"
            fallbacks.append({"actor": name, "slot": index, "source": "Original airport unassigned slot", "replacement": path})
        material = unreal.load_asset(path)
        if not material:
            raise RuntimeError("Original material cannot load: " + path)
        component.set_material(index, material)
    return fallbacks
