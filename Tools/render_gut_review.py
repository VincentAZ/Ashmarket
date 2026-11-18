"""Sets up an in-engine 360 review of one character or prop: a review level lit by one of MetaHuman's own lighting
environments, the character turning on the spot, and a Movie Render Queue preset. render_gut_review.sh then
renders it. The lighting is Epic's, used as shipped; nothing here is part of the game.

Environment: GUT_REVIEW_ACTOR (path of a blueprint or a static mesh), GUT_REVIEW_NAME, GUT_REVIEW_LIGHT (Studio, Portrait, Tungsten, ...),
GUT_REVIEW_BOTTOM / GUT_REVIEW_TOP (slice of the character's height to frame, 0 = feet, 1 = head).
"""
import math
import os

import unreal

FOLDER = "/Game/Gut/Review"
FRAMES = 144
RATE = 24
WIDTH, HEIGHT = 1080, 1350
DISTANCE = 600.0


def log(*parts):
    unreal.log("[render_gut_review] " + " ".join(str(part) for part in parts))


def main():
    blueprint_path = os.environ.get("GUT_REVIEW_ACTOR", "/Game/Gut/Art/MetaHumans/FighterA/BP_FighterA")
    name = os.environ.get("GUT_REVIEW_NAME", "FighterA")
    light = os.environ.get("GUT_REVIEW_LIGHT", "Studio")
    bottom = float(os.environ.get("GUT_REVIEW_BOTTOM", "0"))
    top = float(os.environ.get("GUT_REVIEW_TOP", "1"))

    assets = unreal.EditorAssetLibrary
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    level_path = "%s/Lvl_Review_%s" % (FOLDER, name)
    sequence_path = "%s/Seq_Review_%s" % (FOLDER, name)
    # render_gut_review.sh removes the last review's files first: a level that is still on disk cannot be made anew
    if not levels.new_level(level_path):
        raise RuntimeError("could not create " + level_path)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", unreal.GameModeBase)
    for part in ("L_BaseEnvironment", light):
        added = unreal.EditorLevelUtils.add_level_to_world(
            world, "/MetaHumanCharacter/LightingEnvironments/" + part, unreal.LevelStreamingAlwaysLoaded)
        log("lighting level", part, added)
    # adding a level makes it the current one: go back, or the character is spawned into Epic's lighting level
    levels.set_current_level_by_name("Lvl_Review_" + name)
    log("lighting actors:", sorted({actor.get_class().get_name() for actor in actors.get_all_level_actors()}))

    blueprint = unreal.load_asset(blueprint_path)
    subject = actors.spawn_actor_from_object(blueprint, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    subject.set_actor_label("ReviewSubject")
    # a fighter can be shown with what it carries and wears: GUT_REVIEW_WEAPON=DA_Gut_Club, GUT_REVIEW_ARMOR=DA_Gut_Armor_RubberVest,...
    if isinstance(subject, unreal.Character):
        # the review floor is only a picture of one: stand still on it instead of falling through
        subject.character_movement.set_editor_property("default_land_movement_mode", unreal.MovementMode.MOVE_NONE)
    if os.environ.get("GUT_REVIEW_WEAPON"):
        subject.set_editor_property("starting_weapon", unreal.load_asset("/Game/Gut/Weapons/" + os.environ["GUT_REVIEW_WEAPON"]))
    if os.environ.get("GUT_REVIEW_ARMOR"):
        subject.set_editor_property("starting_armor", [unreal.load_asset("/Game/Gut/Armor/" + name) for name in os.environ["GUT_REVIEW_ARMOR"].split(",")])
    for component in subject.get_components_by_class(unreal.ActorComponent):
        if isinstance(component, unreal.LODSyncComponent):
            component.set_editor_property("forced_lod", 0)
    log("components:", [(component.get_name(), component.get_class().get_name()) for component in subject.get_components_by_class(unreal.SceneComponent)])
    origin, extent = subject.get_actor_bounds(False)
    low, tall = origin.z - extent.z, extent.z * 2
    log("subject height %.1f cm, feet at %.1f" % (tall, low))
    # a prop whose own origin is part-way up it is stood on the floor
    lift = -low if low < -5 else 0.0
    low += lift

    # frame the chosen slice from straight in front; Unreal's field of view is the horizontal one
    slice_low, slice_high = low + tall * bottom, low + tall * top
    focus = unreal.Vector(0, 0, (slice_low + slice_high) / 2)
    half_tall = (slice_high - slice_low) * 1.14 / 2
    # something wider than it is tall (a truck) must fit across the frame as it turns
    half_tall = max(half_tall, max(extent.x, extent.y) * 1.14 * HEIGHT / WIDTH)
    fov = math.degrees(2 * math.atan(half_tall * WIDTH / HEIGHT / DISTANCE))
    # the character faces +Y in its blueprint, so the camera stands out along +Y and looks back
    camera = actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, DISTANCE, focus.z), unreal.Rotator(0, 0, -90))
    camera.set_actor_label("ReviewCamera")
    lens = camera.camera_component
    lens.set_editor_property("field_of_view", fov)
    lens.set_editor_property("constrain_aspect_ratio", False)
    settings = lens.get_editor_property("post_process_settings")
    settings.set_editor_property("override_motion_blur_amount", True)
    settings.set_editor_property("motion_blur_amount", 0.0)
    lens.set_editor_property("post_process_settings", settings)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    sequence = tools.create_asset("Seq_Review_" + name, FOLDER, unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
    sequence.set_display_rate(unreal.FrameRate(RATE, 1))
    sequence.set_playback_start(0)
    sequence.set_playback_end(FRAMES)

    # the sequence spawns its own copies of the character and the camera: actors left in the level were not found at render time
    def placed(actor, values):
        binding = sequence.add_spawnable_from_instance(actor)
        section = binding.add_track(unreal.MovieScene3DTransformTrack).add_section()
        section.set_range(0, FRAMES)
        channels = section.get_all_channels()
        for channel, value in zip(channels, values):
            channel.set_default(value)
        actors.destroy_actor(actor)
        return binding, channels

    turning, channels = placed(subject, (0, 0, lift, 0, 0, 0, 1, 1, 1))
    channels[5].add_key(unreal.FrameNumber(0), 0.0, interpolation=unreal.MovieSceneKeyInterpolation.LINEAR)
    channels[5].add_key(unreal.FrameNumber(FRAMES), 360.0, interpolation=unreal.MovieSceneKeyInterpolation.LINEAR)
    shot, _ = placed(camera, (0, DISTANCE, focus.z, 0, 0, -90, 1, 1, 1))
    cuts = sequence.add_track(unreal.MovieSceneCameraCutTrack)
    cut = cuts.add_section()
    cut.set_range(0, FRAMES)
    cut.set_camera_binding_id(sequence.get_binding_id(shot))
    levels.save_current_level()
    assets.save_loaded_asset(sequence)

    config = unreal.new_object(unreal.MoviePipelinePrimaryConfig)
    output = config.find_or_add_setting_by_class(unreal.MoviePipelineOutputSetting)
    output.output_directory = unreal.DirectoryPath(os.path.join(unreal.Paths.project_saved_dir(), "Review", name))
    output.file_name_format = "f_{frame_number}"
    output.output_resolution = unreal.IntPoint(WIDTH, HEIGHT)
    output.zero_pad_frame_numbers = 4
    config.find_or_add_setting_by_class(unreal.MoviePipelineDeferredPassBase)
    config.find_or_add_setting_by_class(unreal.MoviePipelineImageSequenceOutput_PNG)
    config.find_or_add_setting_by_class(unreal.MoviePipelineGameOverrideSetting)
    smoothing = config.find_or_add_setting_by_class(unreal.MoviePipelineAntiAliasingSetting)
    smoothing.spatial_sample_count = int(os.environ.get("GUT_REVIEW_SAMPLES", "1"))
    smoothing.temporal_sample_count = 1
    smoothing.engine_warm_up_count = 96
    smoothing.render_warm_up_count = 48
    saved, problem = unreal.MoviePipelineEditorLibrary.export_config_to_asset(config, FOLDER + "/", "MRQ_Review_" + name, True)
    log("preset", saved, problem)
    log("DONE", level_path, sequence_path)


try:
    main()
except Exception as error:
    log("FAILED", repr(error))
