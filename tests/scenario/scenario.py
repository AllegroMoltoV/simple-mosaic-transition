"""OBS Studio の実機で、モザイクのトランジションを実際の使い方に沿って確かめるシナリオテスト。

設計は docs/scenario-test.md にある。obs-websocket で OBS Studio を操作し、スクリーンショット・録画・統計の値で判定する。
依頼者の既存のシーンと設定を変えないよう、テスト専用のシーンコレクションとプロファイルに切り替えて行い、終了時に元へ戻す。

ポータブル版の OBS Studio で行うときは、tests/scenario/setup-portable.ps1 で準備し、環境変数 OBS_CONFIG_ROOT に
<展開先>\\config\\obs-studio を渡す。

使い方 (リポジトリのルートで実行する):
    .venv/Scripts/python -X utf8 tests/scenario/scenario.py run <出力先>
    .venv/Scripts/python -X utf8 tests/scenario/scenario.py s8-prepare <出力先>
    (tests/scenario/stop-obs.ps1 と start-obs.ps1 で OBS Studio を起動し直す)

手順の全体は docs/scenario-test.md にある。
    .venv/Scripts/python -X utf8 tests/scenario/scenario.py s8-verify <出力先>

終了コードは、合格が 0、不合格が 1、前提の不成立や準備の失敗が 2、トランジションの追加が必要なときが 3。
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path

import obsws_python as obsws
from PIL import Image, ImageChops, ImageStat

PROFILE = "smt-scenario-test"
COLLECTION = "smt-scenario-test"
TRANSITION_KIND = "simple_mosaic_transition"
FADE_KIND = "fade_transition"
SCENES = ("scene-a", "scene-b", "scene-c")
WIDTH, HEIGHT = 1920, 1080
# 判定の閾値。黒は 0 から 255 の輝度の平均、見分けは 16x9 に縮めた画像の平均の差で判定する
BLACK_LUMA = 10.0
SAME_IMAGE = 2.0
SIGNATURE_SIZE = (16, 9)
DEFAULT_SETTINGS = {"dark_window": 50, "max_cell_size": 64, "switch_point": 50}
# OBS Studio の設定のフォルダー。ポータブル版では、環境変数 OBS_CONFIG_ROOT に <展開先>\config\obs-studio を渡す
CONFIG_ROOT = Path(os.environ.get("OBS_CONFIG_ROOT", Path(os.environ["APPDATA"]) / "obs-studio"))
CONFIG = CONFIG_ROOT / "plugin_config" / "obs-websocket" / "config.json"
LOG_DIR = CONFIG_ROOT / "logs"
PLUGIN_ERROR = "[mosaic-transition]"


class PreconditionError(Exception):
    """前提が成り立たず、シナリオを判定できないことを示す"""


class TransitionMissing(Exception):
    """テスト専用のシーンコレクションに、このトランジションがまだ追加されていないことを示す"""


# ---------------------------------------------------------------- 画像の判定


def load(path: Path) -> Image.Image:
    with Image.open(path) as image:
        return image.convert("RGB")


def luma(image: Image.Image) -> float:
    return ImageStat.Stat(image.convert("L")).mean[0]


def diff_mean(a: Image.Image, b: Image.Image) -> float:
    if a.size != b.size:
        b = b.resize(a.size, Image.Resampling.BOX)
    return sum(ImageStat.Stat(ImageChops.difference(a, b)).mean) / 3.0


def signature_distance(a: Image.Image, b: Image.Image) -> float:
    """大きく縮めた画像どうしの差。モザイクはマスの平均を保つため、縮めると元のシーンに近くなる"""
    return diff_mean(a.resize(SIGNATURE_SIZE, Image.Resampling.BOX), b.resize(SIGNATURE_SIZE, Image.Resampling.BOX))


def blockiness(image: Image.Image) -> float:
    """横に隣り合う画素が同じ値になる割合。マスが大きいほど 1 に近づく"""
    gray = image.convert("L")
    diff = ImageChops.difference(gray, ImageChops.offset(gray, 1, 0))
    histogram = diff.histogram()
    return histogram[0] / sum(histogram)


def closer_to(image: Image.Image, ref_a: Image.Image, ref_b: Image.Image) -> str:
    return "a" if signature_distance(image, ref_a) < signature_distance(image, ref_b) else "b"


# ---------------------------------------------------------------- テスト用の画像


def textured(base) -> Image.Image:
    """base(x, y) の色に、隣り合う画素で値が変わる細かい模様を重ねる。元の映像の粗さの指標を低く保ち、モザイクと見分けるため"""
    data = bytearray(WIDTH * HEIGHT * 3)
    i = 0
    for y in range(HEIGHT):
        for x in range(WIDTH):
            r, g, b = base(x, y)
            grain = ((x * 7 + y * 13) % 16) - 8
            data[i] = max(0, min(255, r + grain))
            data[i + 1] = max(0, min(255, g - grain))
            data[i + 2] = max(0, min(255, b + grain))
            i += 3
    return Image.frombytes("RGB", (WIDTH, HEIGHT), bytes(data))


def make_assets(directory: Path) -> dict[str, Path]:
    """前後のシーンを画素で見分けられるよう、色と模様の異なる画像を作る"""
    directory.mkdir(parents=True, exist_ok=True)
    paths = {name: directory / f"{name}.png" for name in ("scene-a", "scene-b", "badge")}
    textured(lambda x, y: (220, 70 + x * 120 // WIDTH, 40) if (x // 40 + y // 40) % 2 == 0
             else (100, 30, 30 + y * 60 // HEIGHT)).save(paths["scene-a"])
    textured(lambda x, y: (40, 90, 210) if (x // 30) % 2 == 0 else (230, 200, 50)).save(paths["scene-b"])
    Image.new("RGB", (240, 240), (255, 255, 255)).save(paths["badge"])
    return paths


# ---------------------------------------------------------------- OBS Studio の操作


@dataclass
class Session:
    client: obsws.ReqClient
    out: Path
    transition: str = ""
    fade: str = ""
    shot_count: int = 0
    original_collection: str = ""
    original_profile: str = ""
    original_studio: bool = False
    results: list[dict] = field(default_factory=list)

    def send(self, request: str, data: dict | None = None) -> dict:
        return self.client.send(request, data, raw=True) or {}

    def stats(self) -> dict:
        return self.send("GetStats")

    def program(self, scene: str, wait: float = 0.0) -> None:
        self.send("SetCurrentProgramScene", {"sceneName": scene})
        time.sleep(wait)

    def use_transition(self, settings: dict, duration_ms: int = 1000) -> None:
        self.send("SetCurrentSceneTransition", {"transitionName": self.transition})
        self.send("SetCurrentSceneTransitionDuration", {"transitionDuration": duration_ms})
        self.send("SetCurrentSceneTransitionSettings", {"transitionSettings": settings, "overlay": True})

    def studio(self, enabled: bool) -> None:
        self.send("SetStudioModeEnabled", {"studioModeEnabled": enabled})
        time.sleep(0.5)

    def screenshot(self, name: str) -> Image.Image:
        """OBSBasic.Screenshot のホットキーでプログラム出力を撮影する。保存先はプロファイルの録画の保存先"""
        capture_dir = self.out / "capture"
        before = set(capture_dir.glob("*.png"))
        self.send("TriggerHotkeyByName", {"hotkeyName": "OBSBasic.Screenshot"})
        deadline = time.monotonic() + 10.0
        while time.monotonic() < deadline:
            created = set(capture_dir.glob("*.png")) - before
            if created:
                path = created.pop()
                size = -1
                while size != path.stat().st_size:
                    size = path.stat().st_size
                    time.sleep(0.3)
                self.shot_count += 1
                target = self.out / f"{self.shot_count:02d}-{name}.png"
                path.replace(target)
                return load(target)
            time.sleep(0.1)
        raise PreconditionError(f"screenshot '{name}' was not saved within 10 seconds")

    def wait_transition_end(self, timeout: float = 10.0) -> None:
        """前のトランジションが終わるまで待つ。OBS Studio はトランジションの最中に T バーの操作を受け付けない
        (OBS 33.0.1 frontend/widgets/OBSBasic_Transitions.cpp:338-344)。OBS Studio と同じく、進み具合が 0 と 1 の間に
        あるときだけトランジションの最中とみなす。手動のトランジションを取り消したあとは 0 が返る"""
        def ended() -> bool:
            cursor = self.send("GetCurrentSceneTransitionCursor")["transitionCursor"]
            return not 0.0 < cursor < 1.0

        wait_until(ended, "the previous transition to end", timeout)

    def prepare_tbar(self) -> None:
        """プログラムを scene-a、プレビューを scene-b にしたスタジオモードにする。
        スタジオモードの中でプログラムを切り替えるとトランジションが走り、その最中の T バーの操作は無視されるため、
        プログラムは通常のモードで切り替え、トランジションが終わってからスタジオモードに入る"""
        self.program("scene-a")
        self.wait_transition_end()
        time.sleep(0.5)
        self.studio(True)
        self.send("SetCurrentPreviewScene", {"sceneName": "scene-b"})
        time.sleep(0.5)
        self.wait_transition_end()

    def tbar(self, position: float, wait: float = 0.8) -> None:
        self.send("SetTBarPosition", {"position": position, "release": False})
        time.sleep(wait)

    def tbar_reset(self) -> None:
        self.send("SetTBarPosition", {"position": 0.0, "release": True})
        time.sleep(0.5)
        self.wait_transition_end()

    def tbar_shots(self, prefix: str, positions: list[float]) -> dict[float, Image.Image]:
        """スタジオモードでプログラムを scene-a、プレビューを scene-b にし、T バーを止めるたびに撮影する"""
        self.prepare_tbar()
        shots = {}
        for i, position in enumerate(positions):
            self.tbar(position)
            shots[i] = self.screenshot(f"{prefix}-{position:.2f}")
        self.tbar_reset()
        self.studio(False)
        return shots

    def record(self, name: str, action) -> Path:
        self.send("StartRecord")
        time.sleep(1.0)
        action()
        time.sleep(0.5)
        path = Path(self.send("StopRecord")["outputPath"])
        time.sleep(1.0)
        target = self.out / f"{name}{path.suffix}"
        path.replace(target)
        return target


def frames_of(video: Path) -> list[Image.Image]:
    """録画を 60 fps の 192x108 の画像に分ける"""
    frame_dir = video.with_suffix("")
    frame_dir.mkdir(exist_ok=True)
    subprocess.run(
        ["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-i", str(video), "-vf", "fps=60,scale=192:108",
         str(frame_dir / "%04d.png")],
        check=True,
    )
    return [load(p) for p in sorted(frame_dir.glob("*.png"))]


def connect(out: Path) -> Session:
    # パスワードは設定ファイルから実行時に読み、ファイルにも出力にも残さない
    config = json.loads(CONFIG.read_text(encoding="utf-8"))
    client = obsws.ReqClient(host="localhost", port=config["server_port"], password=config["server_password"],
                             timeout=10)
    return Session(client=client, out=out)


def wait_until(condition, what: str, timeout: float = 15.0) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if condition():
            return
        time.sleep(0.3)
    raise PreconditionError(f"{what} did not become current within {timeout:.0f} seconds")


def setup(session: Session) -> None:
    """テスト専用のプロファイルとシーンコレクションへ切り替え、シーンを用意する"""
    session.original_collection = session.send("GetSceneCollectionList")["currentSceneCollectionName"]
    profiles = session.send("GetProfileList")
    session.original_profile = profiles["currentProfileName"]
    session.original_studio = session.send("GetStudioModeEnabled")["studioModeEnabled"]
    # Create* は作ったものへ切り替えるが、一覧への反映は UI のスレッドで後から行われる。切り替わるまで待つ
    if PROFILE in profiles["profiles"]:
        session.send("SetCurrentProfile", {"profileName": PROFILE})
    else:
        session.send("CreateProfile", {"profileName": PROFILE})
    wait_until(lambda: session.send("GetProfileList")["currentProfileName"] == PROFILE, f"profile {PROFILE}")
    capture_dir = session.out / "capture"
    capture_dir.mkdir(parents=True, exist_ok=True)
    for category, name, value in (
        ("Output", "Mode", "Simple"),
        ("SimpleOutput", "FilePath", str(capture_dir)),
        ("SimpleOutput", "RecFormat2", "mkv"),
    ):
        session.send("SetProfileParameter", {"parameterCategory": category, "parameterName": name, "parameterValue": value})
    set_canvas(session, WIDTH, HEIGHT)
    if COLLECTION in session.send("GetSceneCollectionList")["sceneCollections"]:
        session.send("SetCurrentSceneCollection", {"sceneCollectionName": COLLECTION})
    else:
        session.send("CreateSceneCollection", {"sceneCollectionName": COLLECTION})
    wait_until(lambda: session.send("GetSceneCollectionList")["currentSceneCollectionName"] == COLLECTION,
               f"scene collection {COLLECTION}")
    time.sleep(1.0)
    assets = make_assets(session.out / "assets")
    existing = {s["sceneName"] for s in session.send("GetSceneList")["scenes"]}
    inputs = {i["inputName"] for i in session.send("GetInputList")["inputs"]}
    for scene in SCENES:
        if scene not in existing:
            session.send("CreateScene", {"sceneName": scene})
    for scene, name, kind, settings in (
        ("scene-a", "image-a", "image_source", {"file": str(assets["scene-a"])}),
        ("scene-a", "badge", "image_source", {"file": str(assets["badge"])}),
        ("scene-b", "image-b", "image_source", {"file": str(assets["scene-b"])}),
        ("scene-c", "color-c", "color_source_v3", {"color": 0xFF30B040, "width": WIDTH, "height": HEIGHT}),
    ):
        if name not in inputs:
            session.send("CreateInput", {"sceneName": scene, "inputName": name, "inputKind": kind, "inputSettings": settings})
    transitions = session.send("GetSceneTransitionList")["transitions"]
    session.transition = next((t["transitionName"] for t in transitions if t["transitionKind"] == TRANSITION_KIND), "")
    session.fade = next((t["transitionName"] for t in transitions if t["transitionKind"] == FADE_KIND), "")
    if not session.transition:
        raise TransitionMissing(TRANSITION_KIND)


def set_canvas(session: Session, width: int, height: int) -> None:
    session.send(
        "SetVideoSettings",
        {"baseWidth": width, "baseHeight": height, "outputWidth": width, "outputHeight": height,
         "fpsNumerator": 60, "fpsDenominator": 1},
    )


def teardown(session: Session) -> None:
    """元のシーンコレクションとプロファイルへ戻す。テスト専用のものは残す"""
    steps = [("SetStudioModeEnabled", {"studioModeEnabled": session.original_studio})]
    # 元の名前を取得する前に失敗したときは、切り替えていないため戻さない
    if session.original_collection:
        steps.append(("SetCurrentSceneCollection", {"sceneCollectionName": session.original_collection}))
    if session.original_profile:
        steps.append(("SetCurrentProfile", {"profileName": session.original_profile}))
    for request, data in steps:
        try:
            session.send(request, data)
            time.sleep(1.0)
        except Exception as error:  # 後始末の失敗は報告して続ける
            print(f"teardown: {request} failed: {error}", file=sys.stderr)


# ---------------------------------------------------------------- シナリオ


@dataclass
class Check:
    session: Session
    scenario: str
    failures: list[str] = field(default_factory=list)
    notes: dict = field(default_factory=dict)

    def expect(self, condition: bool, message: str) -> None:
        if not condition:
            self.failures.append(message)


def run_scenario(session: Session, name: str, body) -> None:
    check = Check(session, name)
    before = session.stats()
    try:
        body(check)
    except PreconditionError:
        raise
    except Exception as error:
        check.failures.append(f"error: {type(error).__name__}: {error}")
    after = session.stats()
    skipped = after["renderSkippedFrames"] - before["renderSkippedFrames"]
    check.notes["render_skipped_frames"] = skipped
    check.expect(skipped == 0, f"render skipped {skipped} frame(s)")
    session.results.append({"scenario": name, "passed": not check.failures, "failures": check.failures, "notes": check.notes})
    print(f"{'PASS' if not check.failures else 'FAIL'} {name}" + "".join(f"\n  - {f}" for f in check.failures))


def references(session: Session) -> tuple[Image.Image, Image.Image]:
    session.use_transition(DEFAULT_SETTINGS)
    session.program("scene-a", wait=2.0)
    ref_a = session.screenshot("ref-a")
    session.program("scene-b", wait=2.0)
    ref_b = session.screenshot("ref-b")
    session.program("scene-a", wait=2.0)
    return ref_a, ref_b


def preconditions(session: Session, ref_a: Image.Image, ref_b: Image.Image) -> None:
    """P1: T バーを止められる。P2: トランジション中のプログラム出力を撮影できる"""
    session.use_transition({**DEFAULT_SETTINGS, "dark_window": 0})
    session.prepare_tbar()
    session.tbar(0.45, wait=1.0)
    first = session.screenshot("p1-first")
    time.sleep(1.0)
    second = session.screenshot("p1-second")
    session.tbar_reset()
    session.studio(False)
    if diff_mean(first, second) > SAME_IMAGE:
        raise PreconditionError("P1: the T-bar did not stay at 0.45")
    if signature_distance(first, ref_a) < 0.5 or diff_mean(first, ref_a) < SAME_IMAGE:
        raise PreconditionError("P2: the screenshot at T-bar 0.45 equals scene-a; the transition was not captured")
    if closer_to(first, ref_a, ref_b) != "a":
        raise PreconditionError("P2: the screenshot at T-bar 0.45 is not a mosaic of scene-a")


def s1(check: Check, ref_a, ref_b) -> None:
    s = check.session
    s.use_transition(DEFAULT_SETTINGS, 1000)
    s.program("scene-a", wait=1.5)
    video = s.record("s1", lambda: s.program("scene-b", wait=2.0))
    frames = frames_of(video)
    small_a, small_b = ref_a.resize((192, 108)), ref_b.resize((192, 108))
    lumas = [luma(f) for f in frames]
    check.notes["min_luma"] = min(lumas)
    check.expect(closer_to(frames[0], small_a, small_b) == "a", "first frame is not scene-a")
    check.expect(closer_to(frames[-1], small_a, small_b) == "b", "last frame is not scene-b")
    check.expect(min(lumas) < BLACK_LUMA, f"no black frame (min luma {min(lumas):.1f})")


def s2(check: Check, ref_a, ref_b) -> dict:
    s = check.session
    s.use_transition(DEFAULT_SETTINGS)
    positions = [0.25, 0.45, 0.5, 0.75, 0.25]
    shots = s.tbar_shots("s2", positions)
    check.expect(closer_to(shots[0], ref_a, ref_b) == "a", "0.25 is not a mosaic of scene-a")
    check.expect(closer_to(shots[1], ref_a, ref_b) == "a", "0.45 is not a mosaic of scene-a")
    check.expect(blockiness(shots[1]) > blockiness(shots[0]), "cells at 0.45 are not larger than at 0.25")
    check.expect(luma(shots[2]) < BLACK_LUMA, f"0.5 is not black (luma {luma(shots[2]):.1f})")
    check.expect(closer_to(shots[3], ref_a, ref_b) == "b", "0.75 is not a mosaic of scene-b")
    check.expect(diff_mean(shots[4], shots[0]) <= SAME_IMAGE, "0.25 after moving back differs from the first 0.25")
    check.notes["blockiness"] = {str(p): round(blockiness(shots[i]), 3) for i, p in enumerate(positions)}
    return {0.25: shots[0], 0.5: shots[2]}


def s3(check: Check, ref_a, ref_b) -> None:
    s = check.session
    s.use_transition({**DEFAULT_SETTINGS, "dark_window": 0})
    shots = s.tbar_shots("s3", [0.49, 0.51])
    check.expect(closer_to(shots[0], ref_a, ref_b) == "a", "0.49 is not a mosaic of scene-a")
    check.expect(closer_to(shots[1], ref_a, ref_b) == "b", "0.51 is not a mosaic of scene-b")
    check.expect(min(luma(shots[0]), luma(shots[1])) > BLACK_LUMA * 3, "a frame near the switch point is black")
    check.expect(abs(blockiness(shots[0]) - blockiness(shots[1])) < 0.05, "cell sizes differ around the switch point")


def s4(check: Check, ref_a, ref_b) -> None:
    s = check.session
    s.use_transition(DEFAULT_SETTINGS, 1000)
    s.program("scene-a", wait=1.5)
    s.program("scene-b", wait=0.4)
    s.program("scene-c", wait=2.5)
    check.expect(s.send("GetCurrentProgramScene")["currentProgramSceneName"] == "scene-c", "program is not scene-c")
    check.expect(bool(s.send("GetVersion")), "OBS Studio stopped responding")


def open_projectors(session: Session) -> None:
    """プログラム・プレビュー・マルチビューのプロジェクターを開く。ウィンドウを開く間はフレームが落ちることがあるため、
    S5 の統計の基準を取る前に開いて落ち着かせる"""
    for mix in ("PROGRAM", "PREVIEW", "MULTIVIEW"):
        session.send("OpenVideoMixProjector", {"videoMixType": f"OBS_WEBSOCKET_VIDEO_MIX_TYPE_{mix}"})
    time.sleep(3.0)


def s5(check: Check, ref_a, ref_b, s2_shots: dict) -> None:
    s = check.session
    s.use_transition(DEFAULT_SETTINGS)
    shots = s.tbar_shots("s5", [0.25, 0.5])
    check.expect(diff_mean(shots[0], s2_shots[0.25]) <= SAME_IMAGE, "0.25 differs from S2 with projectors open")
    check.expect(diff_mean(shots[1], s2_shots[0.5]) <= SAME_IMAGE, "0.5 differs from S2 with projectors open")


def s6(check: Check, ref_a, ref_b) -> None:
    s = check.session
    if not s.fade:
        check.expect(False, "the fade transition was not found")
        return
    s.send("SetCurrentSceneTransition", {"transitionName": s.fade})
    s.send("SetCurrentSceneTransitionDuration", {"transitionDuration": 300})
    s.send("SetSceneSceneTransitionOverride",
           {"sceneName": "scene-b", "transitionName": s.transition, "transitionDuration": 1500})
    try:
        s.program("scene-a", wait=1.0)
        to_b = frames_of(s.record("s6-to-b", lambda: s.program("scene-b", wait=2.5)))
        to_a = frames_of(s.record("s6-to-a", lambda: s.program("scene-a", wait=1.5)))
    finally:
        s.send("SetSceneSceneTransitionOverride", {"sceneName": "scene-b", "transitionName": None})
    check.notes["min_luma_to_b"] = min(luma(f) for f in to_b)
    check.notes["min_luma_to_a"] = min(luma(f) for f in to_a)
    check.expect(check.notes["min_luma_to_b"] < BLACK_LUMA, "switching to scene-b did not use the mosaic transition")
    check.expect(check.notes["min_luma_to_a"] > BLACK_LUMA * 3, "switching back to scene-a used the mosaic transition")


def s9(check: Check, ref_a, ref_b) -> None:
    s = check.session
    s.use_transition(DEFAULT_SETTINGS, 300)
    before = s.stats()
    for i in range(50):
        s.program("scene-b" if i % 2 == 0 else "scene-a", wait=0.5)
    after = s.stats()
    growth = after["memoryUsage"] - before["memoryUsage"]
    check.notes["memory_growth_mb"] = round(growth, 1)
    check.expect(growth < 50.0, f"memory grew by {growth:.1f} MB")


def s10(check: Check, ref_a, ref_b) -> None:
    s = check.session
    s.use_transition(DEFAULT_SETTINGS)
    set_canvas(s, 1280, 720)
    time.sleep(1.0)
    try:
        shots = s.tbar_shots("s10", [0.45, 0.5])
        check.expect(shots[0].size == (1280, 720), f"screenshot size is {shots[0].size}")
        check.expect(closer_to(shots[0], ref_a, ref_b) == "a", "0.45 is not a mosaic of scene-a at 1280x720")
        check.expect(luma(shots[1]) < BLACK_LUMA, "0.5 is not black at 1280x720")
    finally:
        set_canvas(s, WIDTH, HEIGHT)
        time.sleep(1.0)


def plugin_errors() -> list[str]:
    logs = sorted(LOG_DIR.glob("*.txt"), key=lambda p: p.stat().st_mtime)
    if not logs:
        return ["no OBS Studio log was found"]
    text = logs[-1].read_text(encoding="utf-8", errors="replace")
    return [line for line in text.splitlines() if PLUGIN_ERROR in line]


S8_SETTINGS = {"dark_window": 30, "max_cell_size": 128, "switch_point": 40}


def command_run(session: Session) -> int:
    ref_a, ref_b = references(session)
    preconditions(session, ref_a, ref_b)
    print("PASS preconditions P1 and P2")
    run_scenario(session, "S1", lambda c: s1(c, ref_a, ref_b))
    s2_shots: dict = {}
    run_scenario(session, "S2", lambda c: s2_shots.update(s2(c, ref_a, ref_b)))
    run_scenario(session, "S3", lambda c: s3(c, ref_a, ref_b))
    run_scenario(session, "S4", lambda c: s4(c, ref_a, ref_b))
    run_scenario(session, "S6", lambda c: s6(c, ref_a, ref_b))
    run_scenario(session, "S9", lambda c: s9(c, ref_a, ref_b))
    run_scenario(session, "S10", lambda c: s10(c, ref_a, ref_b))
    # プロジェクターは obs-websocket から閉じられず、開いたままだと以降の描画の負荷が増えるため、最後に行う
    if s2_shots:
        before = session.stats()["renderSkippedFrames"]
        open_projectors(session)
        print(f"opening projectors skipped {session.stats()['renderSkippedFrames'] - before} frame(s)")
        run_scenario(session, "S5", lambda c: s5(c, ref_a, ref_b, s2_shots))
    errors = plugin_errors()
    session.results.append({"scenario": "log", "passed": not errors, "failures": errors, "notes": {}})
    print(f"{'PASS' if not errors else 'FAIL'} log" + "".join(f"\n  - {e}" for e in errors))
    session.use_transition(DEFAULT_SETTINGS)
    return 0 if all(r["passed"] for r in session.results) else 1


def command_s8_prepare(session: Session) -> int:
    session.use_transition(S8_SETTINGS)
    print("S8 prepared. Restart OBS Studio with stop-obs.ps1 and start-obs.ps1, then run s8-verify.")
    return 0


def command_s8_verify(session: Session) -> int:
    session.send("SetCurrentSceneTransition", {"transitionName": session.transition})
    settings = session.send("GetCurrentSceneTransition")["transitionSettings"]
    actual = {key: settings.get(key) for key in S8_SETTINGS}
    passed = actual == S8_SETTINGS
    session.results.append({"scenario": "S8", "passed": passed, "failures": [] if passed else [f"settings after restart: {actual}"], "notes": {}})
    print(f"{'PASS' if passed else 'FAIL'} S8 {actual}")
    session.use_transition(DEFAULT_SETTINGS)
    return 0 if passed else 1


COMMANDS = {"run": command_run, "s8-prepare": command_s8_prepare, "s8-verify": command_s8_verify}


def main() -> int:
    if len(sys.argv) != 3 or sys.argv[1] not in COMMANDS:
        print(__doc__, file=sys.stderr)
        return 2
    out = Path(sys.argv[2]).resolve()
    out.mkdir(parents=True, exist_ok=True)
    session = connect(out)
    code = 2
    try:
        setup(session)
        code = COMMANDS[sys.argv[1]](session)
    except TransitionMissing:
        print("The transition is not in the scene collection 'smt-scenario-test'. Quit OBS Studio with stop-obs.ps1, "
              "add it with add-transition.ps1, start OBS Studio again, then run this command again.", file=sys.stderr)
        code = 3
    except PreconditionError as error:
        print(f"precondition failed: {error}", file=sys.stderr)
        code = 2
    finally:
        teardown(session)
        (out / f"results-{sys.argv[1]}.json").write_text(json.dumps(session.results, indent=2, ensure_ascii=False),
                                                         encoding="utf-8")
        session.client.disconnect()
    return code


if __name__ == "__main__":
    sys.exit(main())
