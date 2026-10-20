"""Adjust only existing map3 lights for readable imported aircraft; no actor recreation."""
import unreal

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.load_level('/Game/Maps/map3')
counts = [0, 0]
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if isinstance(actor, unreal.DirectionalLight):
        actor.light_component.set_editor_property('intensity', 12.0)
        counts[0] += 1
    elif isinstance(actor, unreal.SkyLight):
        actor.light_component.set_editor_property('intensity', 2.0)
        counts[1] += 1
assert counts == [1, 1], counts
assert level.save_current_level()
unreal.log('MAP3_LIGHTING_VERIFIED sun=12 sky=2')
