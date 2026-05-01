from __future__ import annotations

import os
import sys
from pathlib import Path


def _bootstrap_python_paths() -> None:
    root = Path(__file__).resolve().parents[2]
    candidates = []

    dynamo_home = os.environ.get("DYNAMO_HOME")
    if dynamo_home:
        dynamo_home_path = Path(dynamo_home)
        candidates.extend(
            [
                dynamo_home_path / "lib" / "python",
                dynamo_home_path / "ext" / "lib" / "python",
            ]
        )

    candidates.extend(
        [
            root / "tmp" / "dynamo_home" / "lib" / "python",
            root / "tmp" / "dynamo_home" / "ext" / "lib" / "python",
        ]
    )

    for candidate in candidates:
        if candidate.is_dir():
            candidate_str = str(candidate)
            if candidate_str not in sys.path:
                sys.path.insert(0, candidate_str)


_bootstrap_python_paths()

import ddf.ddf_extensions_pb2  # noqa: F401
from google.protobuf import text_format
from google.protobuf.descriptor import FieldDescriptor
from gameobject import gameobject_ddf_pb2
from gamesys import atlas_ddf_pb2
from gamesys import camera_ddf_pb2
from gamesys import gamesys_ddf_pb2
from gamesys import gui_ddf_pb2
from gamesys import label_ddf_pb2
from gamesys import model_ddf_pb2
from gamesys import sound_ddf_pb2
from gamesys import sprite_ddf_pb2
from gamesys import tile_ddf_pb2
from particle import particle_ddf_pb2
from render import material_ddf_pb2
from render import render_ddf_pb2


def is_resource_field(field_desc: FieldDescriptor) -> bool:
    for options_field_desc, value in field_desc.GetOptions().ListFields():
        if options_field_desc.name == "resource" and value:
            return True
    return False


PROTO_BY_EXTENSION = {
    ".atlas": atlas_ddf_pb2.Atlas,
    ".camera": camera_ddf_pb2.CameraDesc,
    ".collection": gameobject_ddf_pb2.CollectionDesc,
    ".collectionfactory": gamesys_ddf_pb2.CollectionFactoryDesc,
    ".collectionproxy": gamesys_ddf_pb2.CollectionProxyDesc,
    ".factory": gamesys_ddf_pb2.FactoryDesc,
    ".go": gameobject_ddf_pb2.PrototypeDesc,
    ".gui": gui_ddf_pb2.SceneDesc,
    ".label": label_ddf_pb2.LabelDesc,
    ".material": material_ddf_pb2.MaterialDesc,
    ".model": model_ddf_pb2.ModelDesc,
    ".particlefx": particle_ddf_pb2.ParticleFX,
    ".render": render_ddf_pb2.RenderPrototypeDesc,
    ".sound": sound_ddf_pb2.SoundDesc,
    ".sprite": sprite_ddf_pb2.SpriteDesc,
    ".tilemap": tile_ddf_pb2.TileGrid,
    ".tilesource": tile_ddf_pb2.TileSet,
}


EMBEDDED_PROTO_BY_TYPE = {
    "camera": camera_ddf_pb2.CameraDesc,
    "collectionfactory": gamesys_ddf_pb2.CollectionFactoryDesc,
    "collectionproxy": gamesys_ddf_pb2.CollectionProxyDesc,
    "factory": gamesys_ddf_pb2.FactoryDesc,
    "go": gameobject_ddf_pb2.PrototypeDesc,
    "label": label_ddf_pb2.LabelDesc,
    "model": model_ddf_pb2.ModelDesc,
    "particlefx": particle_ddf_pb2.ParticleFX,
    "sound": sound_ddf_pb2.SoundDesc,
    "sprite": sprite_ddf_pb2.SpriteDesc,
    "tilemap": tile_ddf_pb2.TileGrid,
}


def parse_text_proto(text: str, message_cls):
    message = message_cls()
    text_format.Merge(text, message)
    return message


def parse_path(path: Path):
    message_cls = PROTO_BY_EXTENSION.get(path.suffix)
    if message_cls is None:
        return None
    return parse_text_proto(path.read_text(encoding="utf-8", errors="ignore"), message_cls)
