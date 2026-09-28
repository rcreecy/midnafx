from pathlib import Path
import sys


patch = Path(sys.argv[1]).read_text(encoding="utf-8")


def require(fragment: str) -> None:
    if fragment not in patch:
        raise SystemExit(f"camera host patch is missing contract fragment: {fragment}")


# Authored events include conversations, item-get scenes, and cutscenes even when
# the JStudio demo camera is absent.
require("dDemo_c::getCamera() != NULL || dComIfGp_event_runCheck()")
# Numeric mode 0 is shared by several camera styles. Modification is intentionally
# limited to the native chase-controller algorithm.
require("camera.Mode() == 0 && camera.Algorithm() == 1")
# Lock-on can retain numeric mode 0 and the chase algorithm. It must still use
# native targeting framing.
require("camera.mpLockonTarget != nullptr || camera.mpLockOnActor != nullptr")
require("camera.mLockLActive")
require("attention != nullptr && attention->Lockon()")
require("int Algorithm() { return mCamParam.Algorythmn(mCamStyle); }")
require("CAMERA_FOV_CONTEXT_CAN_MODIFY")
require("dusk::isCameraDetached()")
# Depth-of-field autofocus reads the rendered look-at target without taking camera ownership.
require("#define CAMERA_SERVICE_MINOR 4u")
require("ModResult (*get_camera_target)(")
require("outInfo->focus_distance = focusDistance")
